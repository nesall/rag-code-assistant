<script lang="ts">
  import * as icons from "@lucide/svelte";
  import FileAttachments from "./FileAttachments.svelte";
  import ContextFiles from "./ContextFiles.svelte";

  interface Props {
    onSendMessage: (
      message: string,
      attachments: File[],
      filenames: string[],
    ) => void;
    loading: boolean;
  }

  let { loading = false, onSendMessage }: Props = $props();

  let input = $state("");

  let attachments: File[] = $state([]);
  let filenames: string[] = [];

  let apis: ModelItem[] = $state([]);

  function onSubmit(e: Event) {
    e.preventDefault();
    if (
      onSendMessage &&
      (input.trim() || 0 < attachments.length || 0 < filenames.length)
    ) {
      onSendMessage(input.trim(), attachments, filenames);
      input = "";
      attachments = [];
    }
  }

  function onTextereaKeyDown(ke: KeyboardEvent) {
    if (ke.key === "Enter" && !ke.shiftKey) {
      ke.preventDefault();
      onSubmit(ke);
    }
  }

  function onContextFiles(files: string[]) {
    filenames = [...files];
  }

  function onAttachments(files: File[]) {
    attachments = [...files];
  }
</script>

<form class="flex flex-col space-y-2 w-full" onsubmit={onSubmit}>
  <div
    class="flex flex-col rounded-xl border-1 p-3 w-full bg-surface-50-950 border-surface-200-800 relative"
    role="presentation"
    tabindex="-1"
  >
    <div class="flex flex-row w-full relative" role="presentation">
      <textarea
        class="text-md outline-none border-none w-full resize-vertical lg:resize-none lg:max-h-48 lg:overflow-y-auto"
        placeholder="Type a message (Shift+Enter to add a new line)"
        id="msg-input"
        dir="auto"
        rows="5"
        maxlength="4000"
        bind:value={input}
        disabled={loading}
        onkeydown={onTextereaKeyDown}
      ></textarea>
      <div class="flex flex-col items-center gap-4 ml-2 absolute right-0 top-0">
        <button
          type="submit"
          class="btn btn-sm preset-filled-primary-500 w-8 h-8 px-0 rounded-full"
          aria-label="Send message"
          disabled={loading || !input.trim()}
        >
          <icons.ArrowUp />
        </button>
      </div>
    </div>
    <div class="absolute2 left2-1 bottom2-1 flex flex-col space-y-2">
      <FileAttachments {loading} bind:attachments />
      <ContextFiles {loading} onChange={onContextFiles} />
    </div>
  </div>
</form>
