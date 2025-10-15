<script lang="ts">
  // import svelteLogo from "./assets/svelte.svg";
  // import viteLogo from "/vite.svg";
  import ChatPanel from "./lib/widgets/ChatPanel.svelte";
  import { Toast } from "@skeletonlabs/skeleton-svelte";
  import { toaster } from "./lib/utils";
  import Toolbar from "./lib/widgets/Toolbar.svelte";
  import Statusbar from "./lib/widgets/Statusbar.svelte";
  import { onMount, setContext } from "svelte";

  let params: ChatParametersType = $state({
    temperature: 0.1,
    settings: { completionApis: [], currentApi: "" },
  });

  onMount(() => {});
</script>

<main
  class="mx-auto flex flex-col space-y-2 items-center justify-center
    w-[100vw] h-[100vh] min-w-sx max-w-[900px] min-h-sx max-h-[100vh]"
>
  <div class="p-4 m-0 flex flex-col w-full h-full">
    <Toolbar bind:params />
    <div class="chatpanel-wrapper flex-grow w-full h-0 overflow-y-auto">
      <ChatPanel chatParams={params} />
    </div>
  </div>
  <div class="w-full">
    <Statusbar bind:params />
  </div>
</main>

<Toast.Group {toaster}>
  {#snippet children(toast)}
    <Toast {toast}>
      <Toast.Message>
        <Toast.Title>{toast.title}</Toast.Title>
        <Toast.Description>{toast.description}</Toast.Description>
      </Toast.Message>
      <Toast.CloseTrigger />
    </Toast>
  {/snippet}
</Toast.Group>
