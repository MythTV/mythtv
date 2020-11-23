<script>
    export let title, action;
    export let icon = "";

    async function handleClick() {
        const query = new URLSearchParams();
        query.append("Action", action);
        const params = {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
                'Accept':       'application/json'
            },
            body: query
        };
        const res = fetch('/Frontend/SendAction', params);
    }
</script>

<div class="item left waves-effect waves-light btn-large" on:click={handleClick}>
    {#if icon === ""}
        {title}
    {:else}
        <i class={icon}></i>
    {/if}
</div>
