# Fulldex relay

One Cloudflare Worker (free tier) with two jobs: the meeting point for online trades
(frontend/hub_link.cpp, `/v1`) and the Theme Gallery (`/v2`: uploads, moderation queue, share codes,
listing, reports). The route list is at the top of worker.js.

## Put it online (once, from this folder, with a free Cloudflare account)

    npx wrangler login
    npx wrangler r2 bucket create fulldex-themes
    npx wrangler secret put MOD_SECRET        (any long random text; signs the moderator links)
    npx wrangler secret put DISCORD_WEBHOOK   (the webhook URL of your moderation channel)
    npx wrangler deploy

It prints its address, e.g. https://fulldex-relay.<you>.workers.dev. Builds get it with
-DFULLDEX_RELAY_URL=<that address>; a test build also reads fulldex-relay.txt beside its settings.

Optional variables (wrangler.toml `[vars]`): PUBLIC_URL (the address used in Discord links),
UPLOAD_LIMIT (uploads per address per hour, default 5), REPORT_HOLD (reporters that hide a theme, default 3).

## Moderating

Each upload is posted to Discord with its preview and Approve / Reject / Download links. A link only opens
a confirmation page; the button on it does the action, so Discord's link previews cannot approve anything.
`Queue` lists everything waiting. A theme gets its share code at upload, but the code only works once approved.

## Try it on this machine first

    printf 'MOD_SECRET=dev\nDISCORD_WEBHOOK=http://127.0.0.1:9911/hook\nUPLOAD_LIMIT=7\n' > .dev.vars
    npx wrangler dev --ip 127.0.0.1        (http://127.0.0.1:8787, R2 is simulated on disk)
    node test.mjs                          (in another window; needs nothing else)
