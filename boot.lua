-- Handler names: spec_h, center_h, up_h, down_h, fwd_h, bwd_h

function center_h(state)
    local led = state and LED.GREEN or LED.RED
    set(led, true)
    sleep(500)
    set(led, false)
end

--[[ BTN table not implemented yet
function spec_h(state)
    if not state then return end
    for i = 1, 4 do
        set(BTN.LEFT, i % 2)
        sleep(15)
    end
end
--]]
