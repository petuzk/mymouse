--[[
Proof of concept: enabling multi-threading for event handlers by using Lua coroutines.

In a real system, the Lua VM will run in a separate Zephyr thread. Events (such as button
state changes) will be supllied from other thread(s) using a FIFO queue. For each received
event, Lua thread will create a new handler coroutine, store it in a limited-size container
and ask Lua VM to start it. All the code blocking operations (e.g. sleep function) have to yield.
The Lua thread then resumes the coroutine when appropriate (depending on the blocking operation type).
If the handler function returns (coroutine becomes dead), it gets removed from the container. The
freed slot can then be occupied by another coroutine. If there is no free slots left in the container,
the event has to wait in the queue.

The code below consists of two sections. One is the user code section, i.e. the code written by the user.
The surrounding code represents the backstage. Firstly, `sleep` function is defined being an alias
for `coroutine.yield`. Then the user code example defines a handler function, which calls print twice
with some delay in between. The delay itself is not implemented in the scope of this PoC. The backstage
code then creates a coroutine which runs user-defined handler. The idea behind it is that calling `sleep`
returns execution control to the backstage allowing it to do other tasks.
--]]

sleep = coroutine.yield

-- user code begin

function btn_handler(arg)
    print("before rest", arg)
    sleep(1)
    print("rested")
end

-- user code end

co = coroutine.create(btn_handler)
print("main", coroutine.resume(co, "arg"))
print("main", coroutine.resume(co))
print("main", coroutine.resume(co))
