import * as Promise from 'bluebird';

export interface IProtocolHandlers {
  [schema: string]: (inputUrl: string) => Promise<string[]>;
}
