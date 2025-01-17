local cassandra = require("cassandra")
local pretty = require("pl.pretty")

local peer = assert(cassandra.new({
    host = "192.168.117.4",
    port = 9042,
    keyspace = "testing",
}))

peer:settimeout(1000)

assert(peer:connect())

local rows = peer:execute("SELECT * FROM testing.data")

ngx.say("<p>hello, world</p>")

pretty.dump(rows)