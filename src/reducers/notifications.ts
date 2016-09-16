import { addNotification, dismissNotification } from '../actions/actions';
import { dismissDialog, showDialog } from '../actions/actions';

import { createReducer } from 'redux-act';
import update = require('react-addons-update');

import { log } from '../util/log';

let counter = 1;

/**
 * reducer for changes to notifications
 */
export const notificationsReducer = createReducer({
  [addNotification]: (state, payload) => {
    if (payload.id === undefined) {
      payload.id = `__auto_${counter++}`;
    }
    return update(state, { notifications: { $push: [ payload ] } });
  },
  [dismissNotification]: (state, payload) => {
    let idx = state.notifications.findIndex((ele) => ele.id === payload);
    return update(state, { notifications: { $splice : [[idx, 1]] } });
  },
  [showDialog]: (state, payload) => {
    return update(state, { dialogs: { $push: [ payload ] } });
  },
  [dismissDialog]: (state, payload) => {
    return update(state, { dialogs: { $splice: [[0, 1]] } });
  },
}, {
  notifications: [],
  dialogs: [],
});