local lucas = require("lucas")
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

local results = lucas.query("SELECT * FROM testing.data", {})
-- pretty.dump(results)

