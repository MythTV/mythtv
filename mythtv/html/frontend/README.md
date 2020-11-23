## Frontend Web App

### Development

This frontend web app is developed using [Svelte](https://svelte.dev)

**NOTE** We are explicitly checking in the built artifacts, so that npm et. al. do not need installing at build or runtime.

#### Get started

Install the dependencies...

```bash
cd mythtv/html/frontend
npm install
```

...then start [Rollup](https://rollupjs.org):

```bash
npm run dev
```

The output CSS & JS will be output to `mythtv/html/apps`. These will need to be copied to the installation directory in order to test them. `make install` should do the trick

#### Building for production mode

Once you have finished development of the app you should create an optimised version of the app:

```bash
npm run build
```

and check-in the resulting build artifacts in `mythtv/html/apps