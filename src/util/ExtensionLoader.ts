import { addNotification } from '../actions/actions';

import initAboutDialog from '../extensions/about_dialog/index';
import initNutsLocal from '../extensions/nuts_local/index';
import initSettingsInterface from '../extensions/settings_interface/index';
import initSettingsUpdate from '../extensions/updater/index';
import { IExtensionInit } from '../types/Extension';
import { IExtensionApi, IExtensionContext } from '../types/IExtensionContext';
import { INotification } from '../types/INotification';
import { log } from '../util/log';
import { showError } from '../util/message';

import { app as appIn, dialog as dialogIn, remote } from 'electron';
import * as fs from 'fs';
import * as path from 'path';
import * as React from 'react';

let app = appIn;
let dialog = dialogIn;

if (remote !== undefined) {
  app = remote.app;
  dialog = remote.dialog;
}

class ExtensionManager {
  private mExtensions: IExtensionInit[];
  private mApi: IExtensionApi;

  constructor() {
    this.mExtensions = this.loadExtensions();
    this.mApi = {
      getState: () => { return {}; },
      showErrorNotification: (message: string, details: string) => {
        dialog.showErrorBox(message, details);
      },
    };
  }

  public setStore<S>(store: Redux.Store<S>) {
    this.mApi.sendNotification = (notification: INotification) => {
      store.dispatch(addNotification(notification));
    };
    this.mApi.showErrorNotification = (message: string, details: string) => {
      showError(store.dispatch, message, details);
    };
    this.mApi.getState = () => store.getState();
  }

  /**
   * retrieve list of all reducers registered by extensions
   */
  public getReducers() {
    let reducers = [];

    let context = this.emptyExtensionContext();

    context.registerReducer = (path: string[], reducer: any) => {
      reducers.push({ path, reducer });
    };

    this.mExtensions.forEach((ext) => ext(context));

    return reducers;
  }

  /**
   * runs the extension init function with the specified register-function
   * set
   * 
   * @param {string} funcName
   * @param {Function} func
   * 
   * @memberOf ExtensionManager
   */
  public apply(funcName: string, func: Function) {
    let context = this.emptyExtensionContext();

    context[funcName] = func;
    this.mExtensions.forEach((ext) => ext(context));
  }

  /**
   * call the "once" function for all extensions. This should really only be called
   * once.
   */
  public doOnce() {
    let context = this.emptyExtensionContext();

    context.once = (callback: () => void) => {
      callback();
    };

    this.mExtensions.forEach((ext) => ext(context));
  }

  private emptyExtensionContext(): IExtensionContext {
    return {
      registerSettings: (title: string, component: React.ComponentClass<any>) => undefined,
      registerIcon: (group: string, icon: string, title: string, action: any) => undefined,
      registerReducer: (path: string[], reducer: any) => undefined,
      once: (): void => undefined,
      api: Object.assign({}, this.mApi),
    };
  }

  private loadDynamicExtension(extensionPath: string): IExtensionInit {
    let indexPath = path.join(extensionPath, 'index.js');
    if (fs.existsSync(indexPath)) {
      return require(indexPath).default;
    } else {
      return undefined;
    }
  }

  private loadDynamicExtensions(extensionsPath: string): IExtensionInit[] {
    if (!fs.existsSync(extensionsPath)) {
      log('info', 'failed to load dynamic extensions, path doesn\'t exist', extensionsPath);
      fs.mkdirSync(extensionsPath);
      return [];
    }

    let res = fs.readdirSync(extensionsPath)
      .filter((name) => fs.statSync(path.join(extensionsPath, name)).isDirectory())
      .map((name) => {
        try {
          return this.loadDynamicExtension(path.join(extensionsPath, name));
        } catch (err) {
          log('warn', 'failed to load dynamic extension', { error: err.message });
          return undefined;
        }
      });
    return res.filter((func: IExtensionInit) => func !== undefined);
  }

  /**
   * retrieves all extensions to the base functionality, both the static
   * and external ones.
   * This loads external extensions from disc synchronously
   * 
   * @returns {IExtensionInit[]}
   */
  private loadExtensions(): IExtensionInit[] {
    const extensionsPath = path.join(app.getPath('userData'), 'plugins');
    return [
      initSettingsInterface,
      initSettingsUpdate,
      initAboutDialog,
      initNutsLocal,
    ].concat(this.loadDynamicExtensions(extensionsPath));
  }

}

export default ExtensionManager;