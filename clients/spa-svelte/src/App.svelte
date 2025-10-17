<script lang="ts">
  // import svelteLogo from "./assets/svelte.svg";
  // import viteLogo from "/vite.svg";
  import ChatPanel from "./lib/widgets/ChatPanel.svelte";
  import { Toast } from "@skeletonlabs/skeleton-svelte";
  import { apiUrl, clog, toaster } from "./lib/utils";
  import Toolbar from "./lib/widgets/Toolbar.svelte";
  import Statusbar from "./lib/widgets/Statusbar.svelte";
  import { onMount } from "svelte";
  import { settings, temperature } from "./lib/store";

  onMount(() => {
    fetchSettings();
  });

  function fetchSettings() {
    fetch(apiUrl("/api/settings"))
      .then((res) => {
        if (!res.ok) throw new Error(`HTTP ${res.status}: ${res.statusText}`);
        return res.json();
      })
      .then((data) => {
        $settings = data as SettingsType;
        let apis = $settings.completionApis;
        console.log("onMount /api/settings:", settings);
        const savedApi = localStorage.getItem("api");
        apis = apis.map((api) => ({
          ...api,
          current: api.id === savedApi,
        }));
        console.log("Toolbar.onMount apis", $state.snapshot(apis));
        $settings.completionApis = apis;
        $settings.currentApi = savedApi || $settings.currentApi;

        const savedTemp = localStorage.getItem("temperature");
        $temperature = Number(savedTemp) || $temperature;
      })
      .catch((err) => {
        clog("Error fetching /api/settings", err.message || err);
      });
  }
</script>

<main
  class="mx-auto flex flex-col space-y-2 items-center justify-center
    w-[100vw] h-[100vh] min-w-sx min-h-sx max-h-[100vh]"
    style="max-width: 60rem"
>
  <div class="p-4 m-0 flex flex-col w-full h-full">
    <Toolbar />
    <div class="chatpanel-wrapper flex-grow w-full h-0 overflow-y-auto">
      <ChatPanel />
    </div>
  </div>
  <div class="w-full">
    <Statusbar {fetchSettings} />
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
