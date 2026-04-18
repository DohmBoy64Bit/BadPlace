print("BadPlace | Starting Request Test Suite (V2 Polling Architecture)...")

-- Test #1: Standard GET request
print("BadPlace | Testing GET: https://httpbin.org/get")
request({
    Url = "https://httpbin.org/get",
    Method = "GET",
    Callback = function(res)
        print("BadPlace | GET Callback Received!")
        print("BadPlace | Status: " .. tostring(res.StatusCode))
        
        if res.StatusCode == 200 then
            print("BadPlace | Success: GET verified.")
            -- Print a small snippet to prove body works
            if res.Body then
                print("BadPlace | Response snippet: " .. string.sub(res.Body, 1, 50) .. "...")
            end
        else
            print("BadPlace | Error: GET failed with code " .. tostring(res.StatusCode))
        end
    end
})

-- Test #2: Standard POST request
print("BadPlace | Testing POST: https://httpbin.org/post")
request({
    Url = "https://httpbin.org/post",
    Method = "POST",
    Body = '{"HelloWorld": "This is from BadPlaceExecutor Polling Branch!"}',
    Headers = {
        ["Content-Type"] = "application/json"
    },
    Callback = function(res)
        print("BadPlace | POST Callback Received!")
        print("BadPlace | Status: " .. tostring(res.StatusCode))
        
        if res.StatusCode == 200 then
            print("BadPlace | Success: POST verified.")
        else
            print("BadPlace | Error: POST failed with code " .. tostring(res.StatusCode))
        end
    end
})

-- Test #3: Yielding test (Prove the Godot Engine is still running code underneath!)
print("BadPlace | Async Check: If you see this message BEFORE the 'Callback Received' messages, then task.spawn yielding is WORKING correctly! \xE2\x9C\x85")

task.spawn(function()
    for i = 1, 3 do
        print("BadPlace | Main Engine simulation tick " .. i .. " (game is not frozen!)")
        task.wait(0.5)
    end
end)
