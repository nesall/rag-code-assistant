<script lang="ts">
  import InputArea from "./InputArea.svelte";
  import * as icons from "@lucide/svelte";
  import { fade } from "svelte/transition";
  import { onMount, tick } from "svelte";
  import DOMPurify from "dompurify";
  import { renderMarkdown } from "../markdown";
  import { toaster } from "../utils";

  interface Props {
    chatParams?: ChatParametersType;
  }
  let { chatParams }: Props = $props();

  interface ChatMessage {
    role: "user" | "assistant" | "system";
    content: string;
    _html: string;
    _userMessage?: string; // for role='user' when there are attachments
    // _improved?: boolean; // user sent an improvement
    // _improvedText?: string;
  }

  async function insertTestMessages() {
    messages = [
      {
        role: "user",
        content: "Hello there!",
        _html: "",
      },
      {
        role: "assistant",
        content: "Hello! How can I assist you today?",
        _html: await renderMarkdown("Hello! How can I assist you today?"),
      },
      {
        role: "user",
        content: "Can you tell me a joke?",
        _html: "",
      },
      {
        role: "assistant",
        content:
          "Sure! Why don't scientists trust atoms? Because they make up everything!",
        _html: await renderMarkdown(
          "Sure! Why don't scientists trust atoms? Because they make up everything!",
        ),
      },
      {
        role: "user",
        content: "Can you tell me a joke?",
        _html: "",
      },
      {
        role: "assistant",
        content:
          "Sure! Why don't scientists trust atoms? Because they make up everything!",
        _html: await renderMarkdown(
          "Sure! Why don't scientists trust atoms? Because they make up everything!",
        ),
      },
    ];
  }

  let loading = $state(false);
  let messagesEndDiv: HTMLDivElement;
  let started = $state(false);
  let messages = $state<ChatMessage[]>([]);
  let showScrollBtn = $state(false);

  function checkMessagesEndVisibility() {
    if (!messagesEndDiv) return;
    const rect = messagesEndDiv.getBoundingClientRect();
    showScrollBtn = window.innerHeight < rect.bottom;
  }

  onMount(() => {
    // insertTestMessages();

    testServerConnection();

    const wrapper = document.querySelector("main") as
      | HTMLDivElement
      | null
      | undefined;
    if (wrapper) wrapper.addEventListener("scroll", checkMessagesEndVisibility);
    window.addEventListener("resize", checkMessagesEndVisibility);
    tick().then(checkMessagesEndVisibility);
    return () => {
      if (wrapper)
        wrapper.removeEventListener("scroll", checkMessagesEndVisibility);
      window.removeEventListener("resize", checkMessagesEndVisibility);
    };
  });

  $effect(() => {
    if (messages) checkMessagesEndVisibility();
  });

  let sourceidsLastUsed: string[] = [];

  function testServerConnection() {
    fetch("/api/health")
      .then((res) => {
        if (!res.ok) throw new Error("Server ping failed");
        return res.text();
      })
      .then((text) => {
        console.log("Server response:", text);
      })
      .catch((err) => {
        console.error("Error connecting to server:", err);
      });
  }

  function onSendMessage(
    message: string,
    attachments: File[],
    sourceids: string[],
  ) {
    if (!message.trim() && attachments.length === 0) return;
    sourceidsLastUsed = [...sourceids];
    message = message.trim();
    if (attachments.length === 0) {
      sendMessage(message, [], sourceids, true);
    } else {
      let atts: { filename: string; content: string }[] = [];
      for (const file of attachments) {
        const reader = new FileReader();
        reader.onload = () => {
          const content = reader.result;
          if (typeof content === "string") {
            // atts += `\n\n[Attachment: ${file.name}]\n${content}\n[/Attachment]`;
            atts.push({ filename: file.name, content });
          }
        };
        reader.onloadend = () => {
          if (attachments.indexOf(file) === attachments.length - 1) {
            console.log("Final input with attachments ready!");
            sendMessage(message, atts, sourceids, true);
          }
        };
        reader.readAsText(file);
      }
    }
  }

  function normalizeHeaders(s: string) {
    return s
      .replace(/<h[1-5]\b([^>]*)>/gi, (_, attrs) => {
        const updatedAttrs = attrs.replace(/class="h[1-5]"/gi, 'class="h6"');
        return `<h5${updatedAttrs}>`;
      })
      .replace(/<\/h[1-5]>/gi, "</h6>");
  }

  function processResponse(s: string) {
    return s
      .replace(/(\n){3,}/g, "\n\n") // Replace 3 or more newlines with 2
      .replace(/^\n+/, "") // Remove leading newlines
      .replace(/\n+$/, ""); // Remove trailing newlines
  }

  function parseFromSSE(chunk: string): string {
    let len = chunk.length;
    if (len === 0) return "";
    let fullResponse: string = "";
    let buffer: string = ""; // holds leftover partial data

    // SSE format: "data: <payload>\n\n"
    buffer += chunk.substring(0, len);
    let pos: number;

    while ((pos = buffer.indexOf("\n\n")) !== -1) {
      const event = buffer.substring(0, pos); // one SSE event
      buffer = buffer.substring(pos + 2);

      if (event.startsWith("data: ")) {
        const jsonStr = event.substring(6);
        if (jsonStr === "[DONE]") {
          break;
        }

        const chunkJson = JSON.parse(jsonStr); // validate JSON
        const content = chunkJson.content || "";
        fullResponse += content;
      }
    }

    return fullResponse;
  }

  async function sendMessage(
    input: string,
    attachments: { filename: string; content: string }[],
    sourceids: string[],
    appendQ = true,
  ) {
    if (loading) return;
    loading = true;
    started = false;
    if (appendQ) {
      if (!input.trim()) return;
      // const _userMessage = input.trim();
      messages = [
        ...messages,
        {
          role: "user",
          // _userMessage,
          content: input.trim(),
          _html: "",
        },
      ];
      input = "";
      tick().then(() => messagesEndDiv?.scrollIntoView({ behavior: "smooth" }));
    }

    try {
      console.log("Sending message to server...", {
        messages,
        attachments,
        sourceids,
        chatParams,
      });
      const response = await fetch("/api/chat", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          messages,
          attachments,
          sourceids,
          targetapi: chatParams?.targetApi,
          temperature: chatParams?.temperature,
        }),
      });
      if (!response.ok) {
        throw new Error("Failed to send message");
      }
      let appended = false;
      const reader = response.body?.getReader();
      const decoder = new TextDecoder();
      while (reader) {
        const { done, value } = await reader.read();
        if (done) break;
        const chunk = parseFromSSE(decoder.decode(value, { stream: true }));
        if (appended) {
          let lm = messages[messages.length - 1];
          lm.content += chunk;
          lm._html = normalizeHeaders(await renderMarkdown(lm.content));
          messages = messages;
          tick().then(checkMessagesEndVisibility);
        } else {
          messages = [
            ...messages,
            {
              role: "assistant",
              content: chunk,
              _html: normalizeHeaders(await renderMarkdown(chunk)),
            },
          ];
          appended = true;
          started = true;
        }
      }
      let lm = messages[messages.length - 1];
      lm.content = processResponse(lm.content);
      lm._html = normalizeHeaders(await renderMarkdown(lm.content));
      // sendFeedback(0, '', '', true);
    } catch (error) {
      console.error("Error sending message:", error);
      messages = [
        ...messages,
        {
          role: "assistant",
          content: "Sorry, there was an error processing your request.",
          _html: "",
        },
      ];
    } finally {
      loading = false;
      started = false;
    }
  }

  function onCopyMsg(content: string) {
    console.log("Copying message:", content);
    navigator.clipboard.writeText(content).then(
      () => {
        toaster.success({ title: "Message copied to clipboard" });
        console.log("Text copied to clipboard");
      },
      (err) => {
        toaster.error({
          title: "Unable to copy message",
          description: err.message,
        });
        console.error("Could not copy text: ", err);
      },
    );
  }

  function onEditMsg(index: number) {
    const msg = messages[index];
    if (msg.role !== "user") return;
    // Populate the input area with the message content for editing
    // This is a placeholder; actual implementation may vary
    toaster.info({ title: "Edit feature not implemented yet." });
  }

  function onRetry(index: number) {
    const msg = messages[index];
    if (msg.role !== "assistant") return;
    if ((index & 1) === 1) {
      const userMsg = messages[index - 1];
      if (userMsg && userMsg.role === "user") {
        messages = messages.slice(0, index);
        sendMessage(userMsg.content, [], sourceidsLastUsed, false);
      } else {
        toaster.error({
          title: "Unpredicted error occurred when retrying an answer.",
        });
      }
    }
  }

  function onThumbsFeedback(index: number, feedback: "good" | "bad") {
    const msg = messages[index];
    if (msg.role !== "assistant") return;
    // Send feedback to the backend or handle it accordingly
    // This is a placeholder; actual implementation may vary
    toaster.info({ title: `Feedback received: ${feedback}` });
  }
</script>

<div
  class="md:p-4 max-w-[900px] w-full h-full p-4 md:p-8 flex flex-col space-y-8"
>
  <div class="flex flex-col space-y-6 mb-4 grow p-4">
    {#if messages.length === 0}
      <p class="text-center text-surface-500">
        No messages yet. Start the conversation!
      </p>
    {/if}
    {#each messages as msg, i}
      {#if msg.role === "user"}
        <div class="flex flex-col items-end overflow-y-hidden box-border">
          <div
            class="bg-primary-50-950 shadow2 rounded-xl whitespace-pre-wrap p-4 break-normal text-left"
            id="user-message-{i}"
          >
            {msg._userMessage || msg.content}
          </div>
          <div class="flex gap-2 mt-1">
            <button
              type="button"
              class="btn btn-sm px-1"
              onclick={() => onCopyMsg(msg._userMessage || msg.content)}
              title="Copy to clipboard"
            >
              <icons.Copy size={16} />
            </button>
            <button
              type="button"
              class="btn btn-sm px-1"
              onclick={() => onEditMsg(i)}
              disabled={loading}
              title="Edit your original question"
            >
              <icons.SquarePen size={16} />
            </button>
          </div>
        </div>
      {:else}
        <div class="flex flex-col overflow-y-hidden box-border pb-4 space-y-1">
          <div
            class="border2 border-surface-100-900 bg-surface-500/5 shadow2 rounded-xl whitespace-normal p-4 break-normal text-left"
          >
            {#if msg._html}
              {@html DOMPurify.sanitize(msg._html, {
                ADD_ATTR: ["onclick"],
              })}
            {:else}
              {msg.content}
            {/if}
          </div>
          <div class="flex items-center">
            <button
              type="button"
              class="btn btn-sm px-2"
              onclick={() => onCopyMsg(msg.content)}
              disabled={loading}
              title="Copy to clipboard"
            >
              <icons.Copy size={16} />
            </button>
            <button
              type="button"
              class="btn btn-sm px-1"
              onclick={() => onThumbsFeedback(i, "good")}
              disabled={loading}
              title="Good answer"
            >
              <icons.ThumbsUp size={16} />
            </button>
            <button
              type="button"
              class="btn btn-sm px-1"
              onclick={() => onThumbsFeedback(i, "bad")}
              disabled={loading}
              title="Incorrect or unhelpful answer"
            >
              <icons.ThumbsDown size={16} />
            </button>
            <span class="vr mx-1 h-[1rem]"></span>
            <button
              type="button"
              class="btn btn-sm px-1"
              onclick={() => onRetry(i)}
              disabled={loading}
              title="Retry the answer"
            >
              <icons.RefreshCw size={16} />
            </button>
          </div>
        </div>
      {/if}
    {/each}
    {#if loading && !started}
      <div class="italic text-gray-500 text-sm">Thinking...</div>
    {/if}
    <div class="min-h-[4rem]"></div>
    <div bind:this={messagesEndDiv}></div>
  </div>

  <div
    class="sticky bottom-0 flex items-end pb-0 pt-4 relative pb-8 gradient-to-t from-surface-50-950"
  >
    {#if showScrollBtn}
      <div class="absolute top-[-1.5rem] w-full flex" transition:fade>
        <button
          type="button"
          class="btn preset-filled-surface-100-900 w-8 h-8 p-0 rounded-full mx-auto"
          aria-label="Scroll to bottom"
          id="scroll-to-bottom-btn"
          onclick={() => messagesEndDiv?.scrollIntoView({ behavior: "smooth" })}
        >
          <icons.ArrowDown />
        </button>
      </div>
    {/if}
    <InputArea {onSendMessage} {loading} />
  </div>
</div>
