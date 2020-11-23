<script>
    import Link from "svelte-routing/src/Link.svelte";
    import RemoteButton from "./RemoteButton.svelte";
    function buttonNumber(num) {
        // Passed 0-8, buttons listed as 1-9
        return (num + 1).toString();
    }
</script>

<style>
    remote {
        background-color: #073642;
    }
    remotelayout {
        grid-template-columns: repeat(3, 80px);
        grid-template-rows: repeat(4, 58px) 10px 58px 10px repeat(3, 58px) 10px repeat(2, 58px);
        width: auto;
        display: grid;
    }
    linktext {
        color: #fdf6e3;
    }
</style>

<remote>
    <!-- Buttons: 0-9, menu, info, back, mute, up, down, left, right, enter, ffwd, rewind-->
    <remotelayout>
        {#each {length: 9} as _, num}
            <RemoteButton title={buttonNumber(num)} action={buttonNumber(num)}/>
        {/each}
        <div class="item left"></div>
        <RemoteButton title="0" action="0"/>
        <div class="item left"></div>
        <div class="item left"></div>
        <div class="item left"></div>
        <div class="item left"></div>
        <RemoteButton title="Menu" action="MENU" icon="fas fa-bars"/>
        <RemoteButton title="Mute" action="MUTE" icon="fas fa-volume-mute"/>
        <RemoteButton title="Info" action="INFO" icon="fas fa-info"/>
        <div class="item left"></div>
        <div class="item left"></div>
        <div class="item left"></div>
        <div class="item left"></div>
        <RemoteButton title="^" action="UP" icon="fas fa-angle-up"/>
        <div class="item left"></div>
        <RemoteButton title="<" action="LEFT" icon="fas fa-angle-left"/>
        <RemoteButton title="ENTER" action="SELECT" icon="fas fa-check"/>
        <RemoteButton title=">" action="RIGHT" icon="fas fa-angle-right"/>
        <RemoteButton title="BACK" action="ESCAPE" icon="fas fa-backspace"/>
        <RemoteButton title="v" action="DOWN" icon="fas fa-angle-down"/>
        <div class="item left"></div>
        <div class="item left"></div>
        <div class="item left"></div>
        <div class="item left"></div>
        <RemoteButton title="<<" action="JUMPRWND" icon="fas fa-angle-double-left"/>
        <RemoteButton title="II" action="PAUSE" icon="fas fa-pause"/>
        <RemoteButton title=">>" action="JUMPFFWD" icon="fas fa-angle-double-right"/>
        <RemoteButton title="SEEKRWND" action="SEEKRWND" icon="fas fa-step-backward"/>
        <RemoteButton title="PLAY" action="PLAY" icon="fas fa-play"/>
        <RemoteButton title="SEEKFFWD" action="SEEKFFWD" icon="fas fa-step-forward"/>
        </remotelayout>
</remote>
<br>
<div class="item left waves-effect waves-light btn-large">
    <Link to="/"><linktext>Back to Status</linktext></Link>
</div>
