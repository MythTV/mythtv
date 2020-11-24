<script>
    import { onMount } from "svelte";
    import { Link } from "svelte-routing";
    import DetailItem from "./DetailItem.svelte";

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
    linktext {
        color: #002b36;
    }
</style>

{#await fe_status}
    <status>
        <h2 class="truncate">MythFrontend Status</h2>
        <DetailItem title="This Frontend">
            <p>loading...</p>
        </DetailItem>
        <DetailItem title="Services">
            <div slot="action">
                <div class="item left waves-effect waves-light btn-small">
                    <Link to="/MythFE/GetRemote"><linktext>Switch to Remote Control</linktext></Link>
                </div>
            </div>
        </DetailItem>
        <DetailItem title="Machine Information">
            <p>loading...</p>
        </DetailItem>
        </status>
{:then Status}
    <status>
        <h2 class="truncate">MythFrontend Status</h2>
        <DetailItem title="This Frontend">
            Name: {Status.Name}
            <br>
            Version: {Status.Version}
        </DetailItem>
        <DetailItem title="Services">
            <div slot="action">
                <div class="item left waves-effect waves-light btn-small">
                    <Link to="/MythFE/GetRemote"><linktext>Switch to Remote Control</linktext></Link>
                </div>
            </div>
        </DetailItem>
        <DetailItem title="Machine Information">
            <span>
                The current frontend status is: {Status.State.state}
                {#if Status.State.state == "WatchingPreRecorded"}
                    , Title: {Status.State.title}, Subtitle: {Status.State.subtitle}
                {/if}
            </span>
        </DetailItem>
    </status>
{/await}

