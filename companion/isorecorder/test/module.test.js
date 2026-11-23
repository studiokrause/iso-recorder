import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import dgram from 'node:dgram'
import { test } from 'node:test'
import { validateManifest } from '@companion-module/base/manifest'
import IsoRecorderInstance, { UpgradeScripts } from '../src/main.js'
import { IsoRecorderClient, variableId } from '../src/client.js'
import { decode, encode } from '../src/osc.js'
import { buildPresets } from '../src/presets.js'

test('manifest is valid', () => {
	const manifest = JSON.parse(readFileSync(new URL('../companion/manifest.json', import.meta.url), 'utf8'))
	validateManifest(manifest, true)
	assert.ok(Array.isArray(UpgradeScripts))
})

test('OSC codec: round trip, padding, bundle, damaged data', () => {
	const b = encode('/isorec/ch/3/state', ['REC', 7, 1.5])
	assert.equal(b.length % 4, 0)
	const [m] = decode(b)
	assert.equal(m.path, '/isorec/ch/3/state')
	assert.deepEqual(m.args, ['REC', 7, 1.5])
	assert.deepEqual(decode(encode('/isorec/status')), [{ path: '/isorec/status', args: [] }])
	const a = encode('/a'), c = encode('/c', [1])
	const len = (x) => { const l = Buffer.alloc(4); l.writeInt32BE(x.length); return l }
	const bundle = Buffer.concat([Buffer.from('#bundle\0'), Buffer.alloc(8), len(a), a, len(c), c])
	assert.deepEqual(decode(bundle).map((x) => x.path), ['/a', '/c'])
	assert.deepEqual(decode(Buffer.from('abc')), [])
	assert.deepEqual(decode(Buffer.from('/x\0\0,i\0\0')), [])
})

test('variable ids', () => {
	assert.equal(variableId('/isorec/ch/3/state'), 'ch3_state')
	assert.equal(variableId('/isorec/rec/count'), 'rec_count')
	assert.equal(variableId('/isorec/view'), 'view')
	assert.equal(variableId('/other/x'), null)
})

test('every preset uses existing actions, feedbacks and variables', () => {
	const inst = Object.create(IsoRecorderInstance.prototype)
	const actions = inst.actions()
	const feedbacks = inst.feedbacks()
	const variables = inst.variables()
	const { structure, presets } = buildPresets('isorec')
	const ids = new Set(Object.keys(presets))
	for (const s of structure) for (const g of s.definitions) for (const p of g.presets) assert.ok(ids.has(p), `group refers to missing preset ${p}`)
	assert.ok(Object.keys(presets).length >= 90)
	for (const [id, p] of Object.entries(presets)) {
		for (const step of p.steps) for (const a of step.down) {
			assert.ok(actions[a.actionId], `${id}: unknown action ${a.actionId}`)
			for (const o of actions[a.actionId].options) {
				const choice = a.options[o.id]
				assert.notEqual(choice, undefined, `${id}: option ${o.id} missing`)
				if (o.choices) assert.ok(o.choices.some((c) => c.id === choice), `${id}: ${o.id}=${choice} not among the choices`)
			}
		}
		for (const f of p.feedbacks) {
			assert.ok(feedbacks[f.feedbackId], `${id}: unknown feedback ${f.feedbackId}`)
			for (const o of feedbacks[f.feedbackId].options) {
				const choice = f.options[o.id]
				if (o.choices) assert.ok(o.choices.some((c) => c.id === choice), `${id}: ${o.id}=${choice} not among the choices`)
			}
		}
		for (const m of p.style.text.matchAll(/\$\(isorec:(\w+)\)/g)) assert.ok(variables[m[1]], `${id}: unknown variable ${m[1]}`)
	}
})

test('actions send the documented OSC paths', async () => {
	const server = dgram.createSocket('udp4')
	const got = []
	server.on('message', (m) => got.push(decode(m)[0].path))
	await new Promise((r) => server.bind(0, '127.0.0.1', r))
	const inst = Object.create(IsoRecorderInstance.prototype)
	inst.client = new IsoRecorderClient()
	await inst.client.open({ host: '127.0.0.1', port: server.address().port })
	const a = inst.actions()
	await a.channel_cmd.callback({ options: { channel: '2', command: 'split' } })
	await a.channel_cmd.callback({ options: { channel: '3', command: 'select/on' } })
	await a.selected_cmd.callback({ options: { command: 'pause' } })
	await a.selection.callback({ options: { what: 'invert' } })
	await a.view.callback({ options: { view: '3x3' } })
	await new Promise((r) => setTimeout(r, 300))
	inst.client.close()
	server.close()
	assert.deepEqual(got.slice(1), ['/isorec/ch/2/split', '/isorec/ch/3/select/on', '/isorec/selected/pause', '/isorec/select/invert', '/isorec/view/3x3'])
	assert.equal(got[0], '/isorec/status')
})
