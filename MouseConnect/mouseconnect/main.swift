//
//  main.swift
//  MouseConnect
//
//  Created by Taras Radchenko on 22/12/2021.
//

import ArgumentParser
import CoreBluetooth

struct Command {
    struct Main: ParsableCommand {
        static var configuration: CommandConfiguration {
            .init(
                commandName: "mouseconnect",
                abstract: "A utility to sync file with the mouse",
                subcommands: [Command.Pull.self, Command.Push.self, Command.MTU.self]
            )
        }
    }
    
    struct MTU: ParsableCommand {
        static var configuration: CommandConfiguration {
            .init(
                commandName: "mtu",
                abstract: "Get peripheral's NSU maximum transmission unit"
            )
        }
        
        func run() throws {
            print("connecting...")
            let comm = MouseCommunicator()
            comm.waitForConnection()
            
            let data = Data([
                Character("m").asciiValue!,
                Character("t").asciiValue!,
                Character("u").asciiValue!])
            comm.push(data)
            
            let resp = comm.pull()
            guard let resp = resp else {
                return
            }
            
            let mtu = UInt32(resp[0]) + UInt32(resp[1]) << 8 + UInt32(resp[2]) << 16 + UInt32(resp[3]) << 24
            print("mtu: \(mtu)")
        }
    }
    
    struct Pull: ParsableCommand {
        static var configuration: CommandConfiguration {
            .init(
                commandName: "pull",
                abstract: "Pull a script file from the mouse"
            )
        }

        @Argument(help: "Destination file name")
        var local_filename: String
        
        @Flag(name: .shortAndLong, help: "Overwrite destination file if exists")
        var force: Int

        func run() throws {
            if FileManager.default.fileExists(atPath: local_filename) && (force == 0) {
                print("error: destination file exists, use -f to overwrite")
                Foundation.exit(1)
            }
            
            print("connecting...")
            let comm = MouseCommunicator()
            comm.waitForConnection()
            
            var data = Data(count: 2)
            data[0] = Character("r").asciiValue!
            data[1] = Character("d").asciiValue!
            comm.push(data)
            
            guard let header = comm.pull() else {
                print("error: nil received for header")
                Foundation.exit(1)
            }
            
            if (header.count < 6 || header[0] != Character("w").asciiValue! || header[1] != Character("r").asciiValue!) {
                print("error: bad header")
                Foundation.exit(1)
            }
            
            let size = UInt32(header[2]) + UInt32(header[3]) << 8 + UInt32(header[4]) << 16 + UInt32(header[5]) << 24
            var contents = Data(capacity: Int(size))
            contents.append(header.advanced(by: 6))
        
            while (contents.count < size) {
                guard let newdata = comm.pull() else {
                    print("error: nil received for data")
                    Foundation.exit(1)
                }
                contents.append(newdata)
            }
            
            print("saving to \(local_filename)...")
            FileManager.default.createFile(atPath: local_filename, contents: contents)
        }
    }
    
    struct Push: ParsableCommand {
        static var configuration: CommandConfiguration {
            .init(
                commandName: "push",
                abstract: "Push a script file to the mouse"
            )
        }

        @Argument(help: "Local file to be pushed")
        var local_filename: String

        func run() throws {
            if !FileManager.default.fileExists(atPath: local_filename) {
                print("error: source file does not exist")
                Foundation.exit(1)
            }
            
            print("connecting...")
            let comm = MouseCommunicator()
            comm.waitForConnection()
            
            guard let size = try? FileManager.default.attributesOfItem(atPath: local_filename)[.size] else {
                print("error: can't retrieve source file size")
                Foundation.exit(1)
            }
            
            var data = Data(count: 6)
            data[0] = Character("w").asciiValue!
            data[1] = Character("r").asciiValue!
            data[2] = UInt8(((size as! UInt64) >>  0) & 0xFF)
            data[3] = UInt8(((size as! UInt64) >>  8) & 0xFF)
            data[4] = UInt8(((size as! UInt64) >> 16) & 0xFF)
            data[5] = UInt8(((size as! UInt64) >> 24) & 0xFF)
            
            data.append(FileManager.default.contents(atPath: local_filename)!)
            comm.push(data)
        }
    }
}

struct NUS_UUID {
    static let service = CBUUID(string: "6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    static let tx_char = CBUUID(string: "6e400002-b5a3-f393-e0a9-e50e24dcca9e")//(Write without response)
    static let rx_char = CBUUID(string: "6e400003-b5a3-f393-e0a9-e50e24dcca9e")//(Read/Notify)
}

class MouseCommunicator: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private var centralManager: CBCentralManager!
    private var bt_queue: DispatchQueue
    private var nus_peripheral: CBPeripheral?
    private var nus_service: CBService?
    private var nus_tx_char: CBCharacteristic?
    private var nus_rx_char: CBCharacteristic?
    private var tx: Bool
    private var rx: Bool
    
    override init() {
        nus_peripheral = nil
        nus_service = nil
        nus_tx_char = nil
        nus_rx_char = nil
        bt_queue = DispatchQueue(label: "bt_queue")
        tx = false
        rx = false
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: bt_queue)
    }
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOff:
            print("error: bluetooth is powered off")
            exit(1)
        case .poweredOn:
            let connected = central.retrieveConnectedPeripherals(withServices: [NUS_UUID.service])
            if connected.isEmpty {
                print("error: no connected devices")
                exit(1)
            }
            else if connected.count > 1 {
                print("error: not implemented")
                exit(1)
            }
            else {
                onConnectedPeripheralFound(connected.first!)
            }
        case .unsupported:
            print("error: bluetooth is unsupported")
            exit(1)
        case .unauthorized:
            print("error: the application is not allowed to access bluetooth devices")
            exit(1)
        case .unknown:
            print("error: unknown bluetooth manager state")
            exit(1)
        case .resetting:
            print("info: resetting connection with the system service")
        @unknown default:
            print("unknown error")
            exit(1)
        }
    }
    
    func waitForConnection() {
        while self.nus_tx_char == nil {
            
        }
    }
    
    // MARK: Connection establishment (discovering NUS service & characteristics)
    
    func onConnectedPeripheralFound(_ peripheral: CBPeripheral) {
        let name = peripheral.name ?? "unnamed device (UUID: \(peripheral.identifier.uuidString))"
        print("connected to \(name)")
        
        nus_peripheral = peripheral
        
        centralManager.connect(peripheral, options: nil)
        peripheral.delegate = self
        peripheral.discoverServices([NUS_UUID.service])
    }
     
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        assert(peripheral == nus_peripheral, "unexpected peripheral: \(peripheral)")
        
        guard let services = peripheral.services else {
            print("error: no services discovered")
            exit(1)
        }
        
        guard let service = services.first(where: {$0.uuid.isEqual(to: NUS_UUID.service)}) else {
            print("NUS service not found")
            exit(1)
        }
        
        nus_service = service
        peripheral.discoverCharacteristics([NUS_UUID.tx_char, NUS_UUID.rx_char], for: service)
    }
     
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        assert(peripheral == nus_peripheral, "unexpected peripheral: \(peripheral)")
        assert(service == nus_service, "unexpected service: \(service)")
        
        guard let characteristics = service.characteristics else {
            print("no characteristics discovered in NUS service")
            exit(1)
        }
        
        guard let tx_char = characteristics.first(where: {$0.uuid.isEqual(to: NUS_UUID.tx_char)}),
              let rx_char = characteristics.first(where: {$0.uuid.isEqual(to: NUS_UUID.rx_char)}) else {
            print("NUS TX/RX characteristics not found")
            exit(1)
        }
        
        nus_tx_char = tx_char
        nus_rx_char = rx_char
        print("service discovery succeeded")
        
        peripheral.setNotifyValue(true, for: rx_char)
    }
    
    // MARK: Data writing
    
    func push(_ data: Data) {
        guard let nus_peripheral = nus_peripheral, let nus_tx_char = nus_tx_char else {
            return
        }

        // todo: mtu=62 should not be hardcoded
        let mtu = min(nus_peripheral.maximumWriteValueLength(for: .withResponse), 62)
        if data.count > mtu {
            for i in stride(from: 0, to: data.count, by: mtu) {
                push(data.subdata(in: i..<min(i+mtu, data.count)))
            }
            return
        }

        nus_peripheral.writeValue(data, for: nus_tx_char, type: .withResponse)
        while (!tx) { }
        tx = false
    }
    
    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        print("[callback] write, error: \(String(describing: error))")
        tx = true
    }
    
    // MARK: Data reading
    
    func pull() -> Data? {
        guard let nus_rx_char = nus_rx_char else {
            return nil
        }

        while (!rx) { }
        rx = false
        return nus_rx_char.value
    }
        
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        print("[callback] read, error: \(String(describing: error))")
        rx = error == nil
    }
}

Command.Main.main()
