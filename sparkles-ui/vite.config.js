import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
  plugins: [sveltekit()],
  server: {
    proxy: {
      '/getAddressList': 'http://localhost:8080',
      '/commandSyncAll': 'http://localhost:8080',
      '/commandBlinkAll': 'http://localhost:8080',
      '/commandAnimate': 'http://localhost:8080',
      '/commandAnimationOff': 'http://localhost:8080',
      '/submitPositions': 'http://localhost:8080',
      '/commandBlink': 'http://localhost:8080',
      '/commandMessage': 'http://localhost:8080',
      '/commandSync': 'http://localhost:8080',
      '/getSystemInfo': 'http://localhost:8080',
      '/setTime': 'http://localhost:8080',
      '/setSleepTime': 'http://localhost:8080',
      '/setWakeupTime': 'http://localhost:8080',
      '/toggleLogging': 'http://localhost:8080',
      '/toggleTestMode': 'http://localhost:8080',
      '/commandOTAUpdate': 'http://localhost:8080',
      '/reannounce': 'http://localhost:8080',
      '/resetSystem': 'http://localhost:8080',
      '/factoryReset': 'http://localhost:8080',
      '/setSyncAsyncParams': 'http://localhost:8080',
      '/getMidiParams': 'http://localhost:8080',
      '/setMidiParams': 'http://localhost:8080',
      '/getDarkroomParams': 'http://localhost:8080',
      '/setDarkroomParams': 'http://localhost:8080',
      '/commandStartCalibration': 'http://localhost:8080',
      '/commandCancelCalibration': 'http://localhost:8080',
      '/commandContinueCalibration': 'http://localhost:8080',
      '/commandResetCalibration': 'http://localhost:8080',
      '/commandEndCalibration': 'http://localhost:8080',
      '/commandStartDistanceCalibration': 'http://localhost:8080',
      '/commandContinueDistanceCalibration': 'http://localhost:8080',
      '/commandAbortDistanceCalibration': 'http://localhost:8080',
      '/commandTestCalibration': 'http://localhost:8080',
      '/events': 'http://localhost:8080'
    }
  }
});
