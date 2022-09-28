const PROXY_CONFIG = [
    {
        context: [
            "/assets",
            "/3rdParty",
            "/images",
            "/Myth",
            "/Guide",
            "/Status",
            "/Config",
            "/Capture",
            "/Content",
            "/Dvr",
            "/Channel",
        ],
        target: "http://localhost:6744",
    }
]

module.exports = PROXY_CONFIG;
