declare interface Window {
  apiServerUrl: string | undefined;
  cppApi: {
    sendServerUrl: (url: string | undefined) => any;
  }
}