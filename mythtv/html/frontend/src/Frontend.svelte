<script>
    import { Router, Route } from "svelte-routing";
    import Home from "./routes/Home.svelte";
    import RemoteControl from "./remote/RemoteControl.svelte";

    export let url= "";
</script>

<style>
    :global(body) {
        font: normal 13px Verdana, Geneva, sans-serif;
        background-color: #657b83;
        border:           0px;
        margin:           0px;
        padding:          0px;
        max-width:        100%;
        overflow-x:       hidden;
        text-overflow:    ellipsis;
    }
</style>

<Router url="{url}">
    <div>
        <Route path="/MythFE/GetRemote" component="{RemoteControl}" />
        <Route path="/" component="{Home}" />
    </div>
</Router>