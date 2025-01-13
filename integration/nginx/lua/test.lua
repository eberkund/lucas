local lucas = require("lucas")
local resty = require("lucas").resty
local compat = require("lucas").compatibility
local pretty = require("pl.pretty")

ngx.say("<p>hello, world</p>")
ngx.log(ngx.INFO, "hello, world")

resty.async_test(function(result, err)
    ngx.log(ngx.INFO, "callback")

    if err then
        ngx.say("Error: ", err)
    else
        ngx.say("Result: ", result)
    end
end)

-- pretty.dump(lucas)
-- pretty.print(lucas)
-- ngx.log(ngx.INFO, "result: ", result)

ngx.say("<p>foo</p>")
