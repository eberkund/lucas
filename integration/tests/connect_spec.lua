local lucas = require("lucas")

describe("connect", function()
    it("no error when server is available", function()
        lucas.connect({
            contact_points = os.getenv("CASSANDRA_HOST"),
            port = os.getenv("CASSANDRA_PORT"),
            credentials = {
                username = os.getenv("CASSANDRA_USER"),
                password = os.getenv("CASSANDRA_PASS"),
            },
        })
    end)

    it("error when server is not available", function()
        assert.has.errors(function()
            lucas.connect({
                contact_points = "example.com",
                connect_timeout = 100,
                credentials = {
                    username = os.getenv("CASSANDRA_USER"),
                    password = os.getenv("CASSANDRA_PASS"),
                },
            })
        end)
    end)

    it("connect with SSL", function()
        lucas.connect({
            contact_points = os.getenv("CASSANDRA_HOST"),
            port = os.getenv("CASSANDRA_PORT_SSL"),
            credentials = {
                username = os.getenv("CASSANDRA_USER"),
                password = os.getenv("CASSANDRA_PASS"),
            },
            ssl = {
                certificate = os.getenv("CASSANDRA_SSL_CERT"),
                private_key = os.getenv("CASSANDRA_SSL_KEY"),
                password = os.getenv("CASSANDRA_SSL_PASS"),
            },
        })
    end)
end)
