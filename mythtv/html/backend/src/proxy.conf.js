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
            "/Video",
        ],
        target: "http://localhost:6544",
    }
]

module.exports = PROXY_CONFIG;
