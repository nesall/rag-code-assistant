<script lang="ts">
  import * as icons from "@lucide/svelte";
  import { Modal } from "@skeletonlabs/skeleton-svelte";
  import { onMount } from "svelte";
  import { apiUrl, clog, getLastLogs } from "../utils";

  interface Props {
    chatParams?: ChatParametersType;
  }
  let { chatParams = $bindable() }: Props = $props();

  interface ModelItem {
    id: string;
    name: string;
    url: string;
    model: string;
    current: boolean;
  }

  interface SettingsType {
    completionApis: ModelItem[];
    currentApi: string;
  }

  let openState = $state(false);
  let apis: ModelItem[] = $state([]);
  let curTheme = $state("cerberus");

  let openLogsState = $state(false);

  let themeOptions = [
    "cerberus",
    "concord",
    "hamlindigo",
    "terminus",
    "mona",
    "wintry",
    "nosh",
    "vox",
    "rocket"
  ];

  onMount(() => {
    // clog("Toolbar mounted");
    if (!chatParams) {
      chatParams = { temperature: 0.4, targetApi: "" };
    }
    fetch(apiUrl("/api/settings"))
      .then((res) => {
        if (!res.ok) throw new Error(`HTTP ${res.status}: ${res.statusText}`);
        return res.json();
      })
      .then((data) => {
        const settings = data as SettingsType;
        apis = settings.completionApis;
        clog("onMount /api/settings:", settings);
        const savedApi = localStorage.getItem("api");
        apis = apis.map((api) => ({
          ...api,
          current: api.id === savedApi,
        }));
        clog("onMount apis", $state.snapshot(apis));
        if (savedApi && chatParams) chatParams.targetApi = savedApi;
      })
      .catch((err) => {
        clog("Error fetching /api/settings", err.message || err);
      });
    try {
      const savedTheme = localStorage.getItem("theme");
      if (savedTheme && themeOptions.includes(savedTheme)) {
        document.documentElement.setAttribute("data-theme", savedTheme);
        curTheme = savedTheme;
      }
      const savedDarkLight = localStorage.getItem("darkOrLight");
      setDarkOrLight(savedDarkLight);

      const savedTemp = localStorage.getItem("temperature");
      if (savedTemp) {
        chatParams.temperature = Number(savedTemp);
      }
    } catch (e) {
      clog("Unable to access localStorage", e);
    }
  });

  $effect(() => {
    if (chatParams) clog("chatParams", $state.snapshot(chatParams));
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
    const newDl =
      htmlEl.getAttribute("data-mode") === "dark" ? "light" : "dark";
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

  function onThemeChange(e: Event) {
    const theme = (e.target as HTMLSelectElement).value;
    document.documentElement.setAttribute("data-theme", theme);
    curTheme = theme;
    try {
      localStorage.setItem("theme", theme);
    } catch (e) {
      clog("Unable to access localStorage", e);
    }
  }

  function onModelChange(e: Event) {
    try {
      const modelId = (e.target as HTMLSelectElement).value;
      localStorage.setItem("api", modelId);
      apis = apis.map((api) => ({
        ...api,
        current: api.id === modelId,
      }));
      clog("Selected model:", modelId);
      chatParams = {
        temperature: chatParams?.temperature || 0.5,
        targetApi: modelId,
      };
    } catch (e) {
      clog("Unable to access localStorage", e);
    }
  }

  function onTempChange(e: Event) {
    if (!chatParams) {
      chatParams = { temperature: 0.5, targetApi: "" };
    }
    try {
      const t = (e.target as HTMLSelectElement).value;
      localStorage.setItem("temperature", t);
      chatParams.temperature = Number(t);
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
</script>

<div
  class="toolbar flex space-x-2 items-center w-full bg-surface-100-900 px-2 py-1 rounded"
>
  <span class="font-semibold text-sm text-surface-700-900">
    RAG Code Assistant
  </span>
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
        curTheme =
          document.documentElement.getAttribute("data-theme") || "cerberus";
        openState = true;
      }}
    >
      <icons.Settings />
    </button>
  </div>
</div>

<Modal
  open={openState}
  onOpenChange={(e) => (openState = e.open)}
  triggerBase="btn preset-tonal"
  contentBase="card bg-surface-100-900/70 p-4 space-y-4 shadow-xl w-sm max-w-screen-sm"
  backdropClasses=""
>
  <!-- {#snippet trigger()}Open Modal{/snippet} -->
  {#snippet content()}
    <header class="flex justify-between">
      <div class="h4">Settings</div>
    </header>
    <hr class="hr" />
    <article class="flex flex-col space-y-4">
      <div class="flex flex-col space-x-2 items-left w-full">
        <span class="whitespace-nowrap">Theme:</span>
        <select class="select w-full px-2 capitalize" onchange={onThemeChange}>
          {#each themeOptions as theme}
            <option
              value={theme}
              selected={theme == curTheme}
              class="capitalize"
            >
              {theme}
            </option>
          {/each}
        </select>
      </div>
      <div class="flex flex-col space-x-2 items-left w-full max-h-[10rem]">
        {#if apis.length === 0}
          <span class="italic text-surface-500">No models available</span>
        {:else}
          <span class="whitespace-nowrap">Model:</span>
          <select class="select w-full px-2" onchange={onModelChange}>
            {#each apis as api, i (api.id)}
              <option value={api.id} selected={api.current}>
                {api.name} - {api.model}
              </option>
            {/each}
          </select>
        {/if}
      </div>
      <div class="flex flex-col space-x-2 items-left w-full">
        <div class="flex justify-between">
          <span class="whitespace-nowrap">Temperature:</span>
          <span class="whitespace-nowrap"
            >({describeTemperature(
              chatParams ? chatParams.temperature : 0.4,
            )})</span
          >
        </div>
        <input
          type="number"
          min="0"
          max="1.0"
          step="0.1"
          class="input w-full px-2"
          value={chatParams?.temperature}
          onchange={onTempChange}
        />
      </div>
    </article>
    <footer class="flex justify-end gap-4">
      <button
        type="button"
        class="mr-auto btn btn-sm hover:text-primary-500"
        onclick={() => (openLogsState = true)}
      >
        Show logs...
      </button>
      <button type="button" class="btn preset-filled" onclick={modalClose}>
        Finish
      </button>
    </footer>
  {/snippet}
</Modal>

<Modal
  open={openLogsState}
  onOpenChange={(e) => (openLogsState = e.open)}
  triggerBase="btn preset-tonal"
  contentBase="card bg-surface-100-900 p-4 space-y-4 shadow-xl w-lg max-w-screen-md"
  backdropClasses=""
>
  {#snippet content()}
    <header class="flex justify-between">
      <div class="h4">Settings</div>
    </header>
    <hr class="hr" />
    <article class="flex flex-col space-y-4">
      <div
        class="whitespace-pre-wrap font-mono text-xs max-h-[60vh] overflow-y-auto"
      >
        <pre id="log-output">
          {#each getLastLogs() as log}<div
              class="flex items-center space-x-1"><span>{log.date}</span><span
                >&nbsp;</span
              ><span>{log.data}</span></div>{/each}
        </pre>
      </div>
    </article>
    <footer class="flex justify-end gap-4">
      <button
        type="button"
        class="btn preset-filled"
        onclick={() => (openLogsState = false)}
      >
        Close
      </button>
    </footer>
  {/snippet}
</Modal>
