<script>
    import { onMount } from "svelte";
    import { Link } from "svelte-routing";

    let fe_status = getStatus();
    async function getStatus() {
        const params = {
            method: 'GET',
            headers: {
                'Accept':       'application/json'
            },
        };
        const res = await fetch('/Frontend/GetStatus', params);
        const data = await res.json();
        if (res.ok) {
            return data.FrontendStatus;
        } else {
            console.log ("failed to load status");
        }
    }
    onMount(getStatus);
</script>

<style>
    status {
        position: absolute;
        top: 64px;
        right: 0px;
        left: 27px;
        width: calc(100% - 27px);
        height: calc(100% - 64px);
        padding: 0px;
        box-sizing: border-box;
    }
    detailbox {
        width: 1000px;
        border-top: 1px solid #002b36;
        border-right: 1px solid #002b36;
        border-bottom: 1px solid #002b36;
        border-left: 10px solid #002b36;
        padding: 10px;
        border-radius: 8px 0px 0px 8px;
        margin: auto;
        margin-bottom: 20px;
        color: #002b36;
        background-color: #93a1a1;
        display: flow-root;
    }
    linktext {
        color: #002b36;
    }
</style>

{#await fe_status}
    <status>
        <p>loading...</p>
    </status>
{:then Status}
    <status>
        <h2 class="center">MythFrontend Status</h2>
        <detailbox>
            <h4>This frontend</h4>
            Name: {Status.Name}
            <br>
            Version: {Status.Version}
        </detailbox>
        <detailbox>
            <h4>Services</h4>
            <div class="item left waves-effect waves-light btn-small">
                <Link to="/MythFE/GetRemote"><linktext>Remote Control</linktext></Link>
            </div>
        </detailbox>
        <detailbox>
            <h4>Machine Information</h4>
            <span>
                The current frontend status is: {Status.State.state}
                {#if Status.State.state == "WatchingPreRecorded"}
                    , Title: {Status.State.title}, Subtitle: {Status.State.subtitle}
                {/if}
            </span>
        </detailbox>
    </status>
{/await}

