function spec_h(state)
    local ledname = state and "green" or "red"
    led(ledname, true)
    sleep(500)
    led(ledname, false)
end
