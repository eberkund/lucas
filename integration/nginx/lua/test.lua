local lucas = require("lucas")
local compat =  require("lucas.compatible")
local resty = require("lucas.resty")

ngx.say("<p>hello, world</p>")
ngx.log(ngx.INFO, "hello, world")

resty.async_test()

-- ngx.log(ngx.INFO, "result: ", result)

ngx.say("<p>foo</p>")
