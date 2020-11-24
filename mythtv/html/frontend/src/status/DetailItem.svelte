<script>
    export let title;
</script>

<div class="row">
    <div class="col s12 m6">
        <div class="card blue-grey darken-1">
            <div class="card-content z-depth-6 white-text">
                <span class="card-title">{title}</span>
                <slot></slot>
                {#if $$slots.action}
                    <div class="card-action">
                        <slot name="action">
                        </slot>
                    </div>
                {/if}
            </div>
        </div>
    </div>
</div>
