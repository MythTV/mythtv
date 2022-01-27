const PROXY_CONFIG = [
    {
        context: [
            "/3rdParty",
            "/images",
            "/Myth",
            "/Status",
            "/Config",
        ],
        target: "http://localhost:6744",
    }
]

module.exports = PROXY_CONFIG;
