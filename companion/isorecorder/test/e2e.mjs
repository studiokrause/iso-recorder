// End-to-end check against a running ISO Recorder (started with --demo 4 --osc-port PORT):
//   node test/e2e.mjs [port]
// Uses the module's own client, so it checks the same code path as Companion does.
import { IsoRecorderClient } from '../src/client.js'

const port = Number(process.argv[2] || 18002)
const c = new IsoRecorderClient()
let bad = 0
const sleep = (ms) => new Promise((r) => setTimeout(r, ms))
const check = (what, ok, got = '') => {
	console.log(`${ok ? 'ok   ' : 'BLAD '} ${what} ${got}`)
	if (!ok) bad++
}
const until = async (fn, ms = 8000) => {
	const t = Date.now()
	while (Date.now() - t < ms) {
		if (fn()) return true
		await sleep(100)
	}
	return false
}
await c.open({ host: '127.0.0.1', port })
check('connected, version and channels', await until(() => c.online && c.values.channels === 4), `v${c.values.version} channels=${c.values.channels}`)
check('names and states', /^TEST/.test(c.values.ch1_name) && c.values.ch1_state === 'PREVIEW', `${c.values.ch1_name} ${c.values.ch1_state}`)
c.send('/isorec/ch/2/rec')
check('ch2 rec -> REC, time runs', await until(() => c.values.ch2_state === 'REC' && /^00:00:0[1-9]/.test(c.values.ch2_time)), `${c.values.ch2_state} ${c.values.ch2_time}`)
check('rec_count = 1', c.values.rec_count === 1, c.values.rec_count)
c.send('/isorec/ch/2/pause')
check('pause', await until(() => c.values.ch2_state === 'PAUSED' && c.values.ch2_paused === 1))
c.send('/isorec/ch/2/stop')
check('stop', await until(() => c.values.ch2_state === 'PREVIEW', 12000))
c.send('/isorec/select/none')
c.send('/isorec/ch/1/select/on')
c.send('/isorec/ch/4/select/on')
check('selection by select/on', await until(() => c.values.selected_count === 2 && c.values.ch4_selected === 1))
c.send('/isorec/selected/rec')
check('selected/rec starts exactly the selected', await until(() => c.values.rec_count === 2) && c.values.ch3_state === 'PREVIEW', `rec=${c.values.rec_count}`)
c.send('/isorec/selected/stop')
check('selected/stop', await until(() => c.values.rec_count === 0, 12000))
c.send('/isorec/select/invert')
check('invert', await until(() => c.values.ch2_selected === 1 && c.values.ch1_selected === 0 && c.values.selected_count === 2))
for (const v of ['2x2', '3x3', 'list', '4x4']) {
	c.send(`/isorec/view/${v}`)
	check(`view ${v}`, await until(() => c.values.view === v), c.values.view)
}
c.close()
console.log(`errors: ${bad}`)
process.exit(bad)
