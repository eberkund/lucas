local lucas = require("lucas")
-- local resty = require("lucas").resty
-- local compat = require("lucas").compatibility
local pretty = require("pl.pretty")

ngx.say("<p>hello, world</p>")
ngx.log(ngx.INFO, "hello, world")

lucas.connect({
    contact_points = "scylla",
    -- port = 9142,
    -- credentials = {
    --     username = "cassandra",
    --     password = "cassandra",
    -- },
})

-- resty.async_test(function(result, err)
--     ngx.log(ngx.INFO, "callback")

--     if err then
--         ngx.say("Error: ", err)
--     else
--         ngx.say("Result: ", result)
--     end
-- end)

