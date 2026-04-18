local WEBHOOK_URL = "https://discord.com/api/webhooks/1461543936077336689/yV-pp6DTMsVhriauvIfw3oiCzqzsoevElH3WkLErmzlCArjAQQPaxdWbCoWXBks6EeIh"

-- Instead of relying on HttpService to JSON encode, we can build the raw JSON string 
-- entirely securely in Lua!
local jsonPayload = [[
{
  "username": "BadPlace Executor",
  "avatar_url": "https://cdn3.emoji.gg/emojis/2662_hacker.png",
  "content": "Testing our new robust HTTP Polyfill!",
  "embeds": [
    {
      "title": "🚀 Execution Success!",
      "description": "The new native Lua polling architecture is fully operational.",
      "color": 5763719,
      "fields": [
        {
          "name": "Engine Status",
          "value": "`Godot Engine 4.x .NET`",
          "inline": true
        },
        {
          "name": "Target Process",
          "value": "`Polytoria Client`",
          "inline": true
        }
      ],
      "footer": {
        "text": "BadPlace Executor System Status"
      }
    }
  ]
}
]]

print("BadPlace | Firing webhook to Discord...")

request({
    Url = WEBHOOK_URL,
    Method = "POST",
    Body = jsonPayload,
    Headers = {
        ["Content-Type"] = "application/json"
    },
    Callback = function(res)
        -- Discord returns 204 No Content exactly when a webhook executes successfully
        if res.StatusCode == 204 then
            print("BadPlace | 🟢 Webhook delivered successfully!")
        else
            print("BadPlace | 🔴 Webhook failed! Status Code: " .. tostring(res.StatusCode))
            if res.Body then
                print("BadPlace | Discord Error: " .. res.Body)
            end
        end
    end
})
