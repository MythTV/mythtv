const PROXY_CONFIG = [
    {
        context: [
            "/3rdParty",
            "/images",
            "/Myth",
            "/Status",
        ],
        target: "http://localhost:6744",
    }
]

module.exports = PROXY_CONFIG;
