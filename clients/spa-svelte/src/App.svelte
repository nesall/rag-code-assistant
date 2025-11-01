<script lang="ts">
  import ChatPanel from "./lib/widgets/ChatPanel.svelte";
  import { Toast } from "@skeletonlabs/skeleton-svelte";
  import { apiUrl, clog, Consts, getPersistentKey, toaster } from "./lib/utils";
  import Toolbar from "./lib/widgets/Toolbar.svelte";
  import Statusbar from "./lib/widgets/Statusbar.svelte";
  import { onMount } from "svelte";
  import { settings, temperature, instances, curInstance } from "./lib/store";

  onMount(() => {
    fetchSettings();
    fetchInstances();
  });

  function fetchSettings() {
    fetch(apiUrl("/api/settings"))
      .then((res) => {
        if (!res.ok) throw new Error(`HTTP ${res.status}: ${res.statusText}`);
        return res.json();
      })
      .then((data) => {
        const ss = data as SettingsType;
        let apis = ss.completionApis;
        console.log("onMount /api/settings:", ss);
        const currentApi = getPersistentKey(Consts.CurrentApiKey) || ss.currentApi;
        apis = apis.map((api) => ({
          ...api,
          current: api.id === currentApi,
        }));
        console.log("Toolbar.onMount apis", $state.snapshot(apis));
        $settings.completionApis = apis;
        $settings.currentApi = currentApi;
        const savedTemp = getPersistentKey(Consts.TemperatureKey);
        $temperature = Number(savedTemp) || $temperature;
      })
      .catch((err) => {
        clog("Error fetching /api/settings", err.message || err);
      });
  }

  async function fetchInstances() {
    const res = await fetch(apiUrl("/api/instances"));
    const data = await res.json();
    console.log("Instances", data);
    $instances = data.instances.map((a: { id: string; host: string; port: number; name: string }) => {
      return {
        value: a.id,
        label: `${a.name}`,
        desc: `${a.host}:${a.port}`,
      };
    });
    $curInstance = data.current_instance;
  }

  function onConnectionStatusChange(ok: boolean) {
    onClear();
  }

  function onClear() {
    chatPanel.resetUi();
  }

  let chatPanel:ChatPanel;
</script>

<main
  class="mx-auto flex flex-col space-y-2 items-center justify-center
    w-[100vw] h-[100vh] min-w-sx min-h-sx max-h-[100vh]"
  style="max-width: 60rem"
>
  <div class="p-4 m-0 flex flex-col w-full h-full">
    <Toolbar {fetchInstances} {onClear} />
    <div class="chatpanel-wrapper flex-grow w-full h-0 overflow-y-auto">
      <ChatPanel bind:this={chatPanel} />
    </div>
  </div>
  <div class="w-full">
    <Statusbar {fetchSettings} {onConnectionStatusChange} />
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
