<script lang="ts">
  // import svelteLogo from "./assets/svelte.svg";
  // import viteLogo from "/vite.svg";
  import ChatPanel from "./lib/widgets/ChatPanel.svelte";
  import { Toaster } from "@skeletonlabs/skeleton-svelte";
  import { toaster } from "./lib/utils";
  import Toolbar from "./lib/widgets/Toolbar.svelte";

  let chatParams: ChatParametersType|undefined = $state();
</script>

<main
  class="mx-auto flex flex-col space-y-2 items-center p-4 w-full h-full justify-center"
>
  <Toolbar bind:chatParams />
  <div class="flex-grow w-full overflow-y-auto">
    <ChatPanel {chatParams} />
  </div>
</main>

<Toaster {toaster} stateError="preset-tonal-error"></Toaster>

<style>
  /* .logo {
    height: 6em;
    padding: 1.5em;
    will-change: filter;
    transition: filter 300ms;
  }
  .logo:hover {
    filter: drop-shadow(0 0 2em #646cffaa);
  }
  .logo.svelte:hover {
    filter: drop-shadow(0 0 2em #ff3e00aa);
  }
  .read-the-docs {
    color: #888;
  } */
</style>
