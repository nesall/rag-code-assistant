declare interface Window {
  apiServerUrl: string | undefined;
  cppApi: {
    sendServerUrl: (url: string | undefined) => any;
    setPersistentKey: (key: string, value: string) => void;
  };
  hljs: {
    highlightAll: () => any;
  }
  HLJS_CUSTOM: {
    initHljs: () => any;
  }
}