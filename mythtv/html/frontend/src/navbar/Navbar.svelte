<script>
    import Logo from "./Logo.svelte";
</script>

<style>
    topbar {
        background-color:   #000000;
        background-image:   linear-gradient(to bottom, #222222, #000000 60%);
        position:           fixed;
        top:                0px;
        right:              0px;
        left:               0px;
        height:             34px;
        border-bottom:      4px solid #004570;
        z-index:            90;
    }
</style>

<topbar>
    <Logo/>
</topbar>
