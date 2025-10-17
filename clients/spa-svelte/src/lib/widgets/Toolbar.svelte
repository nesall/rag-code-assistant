<script lang="ts">
  import * as icons from "@lucide/svelte";
  import { Dialog, Portal } from "@skeletonlabs/skeleton-svelte";
  import { onMount } from "svelte";
  import { clog, getLastLogs } from "../utils";
  import Dropdown from "./Dropdown.svelte";
  import { settings, temperature } from "../store";

  let openState = $state(false);
  let curTheme = $state("cerberus");
  let serverUrl = $state("localhost:8081");

  let openLogsState = $state(false);

  const themeOptions = [
    { label: "Cerberus", value: "cerberus" },
    { label: "Concord", value: "concord" },
    { label: "Hamlindigo", value: "hamlindigo" },
    { label: "Terminus", value: "terminus" },
    { label: "Mona", value: "mona" },
    { label: "Wintry", value: "wintry" },
    { label: "Nosh", value: "nosh" },
    { label: "Vox", value: "vox" },
    { label: "Rocket", value: "rocket" },
  ];

  const apiOptions = $derived(
    $settings.completionApis.map((a) => ({
      value: a.id,
      label: a.name,
      desc: `${a.model} (cost: ${Number(a.combinedPrice).toFixed(2)})`,
    })),
  );

  const curApi = $derived(
    -1 != $settings.completionApis.findIndex((a) => a.current)
      ? $settings.completionApis[$settings.completionApis.findIndex((a) => a.current)].id
      : "",
  );

  onMount(() => {
    console.log("Toolbar onMount");
    try {
      const savedTheme = localStorage.getItem("theme");
      if (savedTheme && -1 != themeOptions.findIndex((a) => a.value == savedTheme)) {
        document.documentElement.setAttribute("data-theme", savedTheme);
        curTheme = savedTheme;
      }
      const savedDarkLight = localStorage.getItem("darkOrLight");
      setDarkOrLight(savedDarkLight);

      serverUrl = localStorage.getItem("serverUrl") || serverUrl;
    } catch (e) {
      clog("Unable to access localStorage", e);
    }
  });

  $effect(() => {
    if ($settings) clog("Toolbar $settings changed:", $state.snapshot($settings));
    if (curApi) clog("Toolbar curApi changed:", $state.snapshot(curApi));
  });

  function setDarkOrLight(dl: string | null) {
    clog("setDarkOrLight", dl);
    const htmlEl = document.documentElement;
    if (dl === "dark") {
      htmlEl.setAttribute("data-mode", "dark");
    } else {
      htmlEl.setAttribute("data-mode", "light");
    }
  }

  function onToggleDarkMode() {
    const htmlEl = document.documentElement;
    const newDl = htmlEl.getAttribute("data-mode") === "dark" ? "light" : "dark";
    setDarkOrLight(newDl);
    try {
      localStorage.setItem("darkOrLight", newDl);
    } catch (e) {
      clog("Unable to access localStorage", e);
    }
  }

  async function modalClose() {
    openState = false;
  }

  function onThemeChange(i: number, theme: string) {
    document.documentElement.setAttribute("data-theme", theme);
    curTheme = theme;
    try {
      localStorage.setItem("theme", theme);
    } catch (e) {
      clog("Unable to access localStorage", e);
    }
  }

  function onModelChange(i: number, modelId: string) {
    try {
      localStorage.setItem("api", modelId);
      $settings.completionApis = $settings.completionApis.map((api) => ({
        ...api,
        current: api.id === modelId,
      }));
      clog("Toolbar.onModelChange", modelId);
      $settings.currentApi = modelId;
    } catch (e) {
      clog("Unable to access localStorage", e);
    }
  }

  function onTempChange(e: Event) {
    try {
      const t = (e.target as HTMLInputElement).value;
      localStorage.setItem("temperature", t);
      $temperature = Number(t);
    } catch (e) {
      clog("Unable to access localStorage", e);
    }
  }

  function describeTemperature(temp: number): string {
    clog("describeTemperature", temp);
    if (temp <= 0.2) return "Precise";
    if (temp <= 0.5) return "Balanced";
    if (temp <= 0.8) return "Creative";
    return "Unpredictable";
  }

  function onDownloadChat() {
    clog("onDownloadChat");
    const chat = document.getElementById("chat-messages");
    if (!chat) return;
    let text = "";
    chat.querySelectorAll(".message").forEach((msg) => {
      const role = msg.getAttribute("data-role") || "user";
      const contentEl = msg.querySelector(".message-content");
      const content = contentEl ? contentEl.textContent || "" : "";
      text += role.toUpperCase() + ":\n" + content + "\n\n";
    });
    const blob = new Blob([text], { type: "text/plain" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `chat_${new Date().toISOString()}.txt`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function onServerUrlChange(e: Event) {
    try {
      serverUrl = (e.target as HTMLInputElement).value;
      localStorage.setItem("serverUrl", serverUrl);
    } catch (e) {
      clog("Unable to access localStorage", e);
    }
  }

  async function onSaveConnection() {
    try {
      let url = `${serverUrl}/api/health`;
      if (!url.startsWith("http")) {
        url = "http://" + url;
      }
      const res = await window.cppApi.sendServerUrl(url);
      clog("onTestConnection", res);
    } catch (error) {
      clog("Saving connection failed:", error);
      alert(`Save failed: ${error instanceof Error ? error.message : "Unknown error"}`);
    }
  }
</script>

<div class="toolbar flex space-x-2 items-center w-full bg-surface-100-900 px-2 py-1 rounded">
  <img src="/logo.png" alt="Logo" class="h-6 w-6" />
  <span class="font-semibold text-sm text-surface-700-900"> RAG Code Assistant </span>
  <!-- <button
    type="button"
    class="btn btn-sm h-8 px-0 rounded-full flex items-center"
    aria-label="New Chat"
    onclick={() => {
      // Start a new chat
      alert("New Chat clicked");
    }}
  >
    <icons.Plus size={16} />
    New Chat
  </button>
  <button
    type="button"
    class="btn btn-sm h-8 px-0 rounded-full flex items-center"
    aria-label="Clear Chats"
    onclick={() => {
      // Clear chat history
      alert("Clear Chats clicked");
    }}
  >
    <icons.Trash2 size={16} />
    Clear Chats
  </button> -->

  <div class="flex space-x-2 items-center ml-auto">
    <button
      type="button"
      class="btn btn-sm btn-icon px-1 hover:preset-tonal"
      aria-label="Settings"
      onclick={onDownloadChat}
      title="Download chat as text file"
    >
      <icons.Download />
    </button>

    <span class="vr h-6"></span>
    <div class="flex space-x-1 items-center px-1">
      <button
        type="button"
        class="btn btn-sm btn-icon px-1 hover:preset-tonal"
        aria-label="Dark/Light Mode"
        onclick={onToggleDarkMode}
      >
        <icons.SunMoon />
      </button>
    </div>
    <button
      type="button"
      class="btn btn-sm btn-icon px-1 hover:preset-tonal"
      aria-label="Settings"
      onclick={() => {
        curTheme = document.documentElement.getAttribute("data-theme") || "cerberus";
        openState = true;
      }}
    >
      <icons.Settings />
    </button>
  </div>
</div>

<Dialog open={openState} onOpenChange={(e) => (openState = e.open)}>
  <Portal>
    <Dialog.Positioner class="fixed inset-0 z-50 flex justify-center items-center">
      <Dialog.Content class="card bg-surface-100-900 w-md p-4 space-y-2 shadow-xl">
        <Dialog.Title class="text-lg font-bold">Settings</Dialog.Title>
        <hr class="hr" />
        <Dialog.Description>
          <div class="flex flex-col space-y-4">
            <div class="flex flex-col space-x-2 items-left w-full">
              <span class="whitespace-nowrap">Theme:</span>
              <Dropdown values={themeOptions} value={curTheme} onChange={onThemeChange} />
            </div>
            <div class="flex flex-col space-x-2 items-left w-full max-h-[10rem]">
              {#if $settings.completionApis.length === 0}
                <span class="italic text-surface-500">No models available</span>
              {:else}
                <span class="whitespace-nowrap">Default model:</span>
                <Dropdown values={apiOptions} value={curApi} onChange={onModelChange} />
              {/if}
            </div>
            <div class="flex flex-col space-x-2 items-left w-full">
              <div class="flex justify-between">
                <span class="whitespace-nowrap">Temperature:</span>
                <span class="whitespace-nowrap">({describeTemperature($temperature)})</span>
              </div>
              <input
                type="number"
                min="0"
                max="1.0"
                step="0.1"
                class="input w-full px-2"
                value={$temperature}
                onchange={onTempChange}
              />
            </div>
            <div class="flex flex-col space-x-2 items-left w-full">
              <span class="whitespace-nowrap">Server:</span>
              <div class="flex items-center space-x-2">
                <input type="url" class="input" value={serverUrl} onchange={onServerUrlChange} />
                <button type="button" class="btn preset-tonal flex items-center" onclick={onSaveConnection}>
                  <icons.CircleCheckBig size={16}/>
                  Save
                </button>
              </div>
            </div>
          </div>
        </Dialog.Description>
        <footer class="flex justify-end gap-4 pt-4">
          <button
            type="button"
            class="mr-auto btn btn-sm hover:text-primary-500 flex-1 justify-start"
            onclick={() => (openLogsState = true)}
          >
            Show logs...
          </button>
          <button type="button" class="btn preset-filled flex-1" onclick={modalClose}> Finish </button>
        </footer>
      </Dialog.Content>
    </Dialog.Positioner>
  </Portal>
</Dialog>

<Dialog open={openLogsState} onOpenChange={(e) => (openLogsState = e.open)}>
  <Portal>
    <Dialog.Backdrop class="" />
    <Dialog.Positioner class="fixed inset-0 z-50 flex justify-center items-center">
      <Dialog.Content class="card bg-surface-100-900 w-xl p-4 space-y-2 shadow-xl">
        <Dialog.Title class="text-lg font-bold">Logs</Dialog.Title>
        <hr class="hr" />
        <Dialog.Description>
          <div class="whitespace-pre-wrap font-mono text-xs max-h-[60vh] overflow-y-auto">
            <pre id="log-output">
          {#each getLastLogs() as log}<div class="flex items-center space-x-1"><span>{log.date}</span><span>&nbsp;</span
                  ><span>{log.data}</span></div>{/each}
        </pre>
          </div>
        </Dialog.Description>
        <Dialog.CloseTrigger class="btn preset-filled w-full">Close</Dialog.CloseTrigger>
      </Dialog.Content>
    </Dialog.Positioner>
  </Portal>
</Dialog>
