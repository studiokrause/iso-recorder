const WHITE = 0xffffff
const RED = 0xcc0000
const DARKRED = 0x4d0000
const AMBER = 0xcc8800
const GREEN = 0x00802b
const BLUE = 0x1f5fbf
const GREY = 0x333333

export const STATES = [
	['REC', 'Recording'],
	['PAUSED', 'Paused'],
	['PREVIEW', 'Preview (not recording)'],
	['WAITING', 'Waiting for the scheduler'],
	['RECONNECTING', 'Reconnecting (signal lost)'],
	['ERROR', 'Error'],
	['SAVING', 'Saving the file'],
	['OFF', 'Input off'],
	['NONE', 'Channel does not exist'],
]

const STATE_STYLE = {
	REC: { bgcolor: RED, color: WHITE },
	PAUSED: { bgcolor: AMBER, color: WHITE },
	WAITING: { bgcolor: BLUE, color: WHITE },
	RECONNECTING: { bgcolor: 0x7a3d00, color: WHITE },
	ERROR: { bgcolor: 0x660000, color: WHITE },
}

const COMMANDS = [
	['rec', 'REC', RED],
	['stop', 'STOP', GREY],
	['pause', 'PAUSE', AMBER],
	['split', 'SPLIT', BLUE],
]

/**
 * Presets (ready-made buttons) for Stream Deck.
 * @param {string} label the connection label, used in $(label:variable)
 */
export function buildPresets(label) {
	const v = (id) => `$(${label}:${id})`
	const presets = {}
	const btn = (name, text, actions, feedbacks = [], bg = GREY) => ({
		type: 'simple',
		name,
		style: { text, size: '14', color: WHITE, bgcolor: bg },
		steps: [{ down: actions, up: [] }],
		feedbacks,
	})
	const group = (id, name, ids) => ({ id, type: 'simple', name, presets: ids })
	const state = (n, s) => ({ feedbackId: 'channel_state', options: { channel: String(n), state: s }, style: STATE_STYLE[s] })

	const sections = []
	for (let first = 1; first <= 16; first += 4) {
		const groups = []
		for (let n = first; n < first + 4; n++) {
			const ids = []
			// tile with the name and the time: red while recording, amber in a pause, green frame when selected
			const tile = `ch${n}_tile`
			presets[tile] = btn(
				`Channel ${n}: status`,
				`${v(`ch${n}_name`)}\n${v(`ch${n}_time`)}`,
				[{ actionId: 'channel_toggle_rec', options: { channel: String(n) } }],
				['REC', 'PAUSED', 'RECONNECTING', 'ERROR', 'WAITING'].map((s) => state(n, s)),
			)
			ids.push(tile)
			for (const [cmd, text, bg] of COMMANDS) {
				const id = `ch${n}_${cmd}`
				const fb = cmd === 'rec' ? [state(n, 'REC')] : cmd === 'pause' ? [state(n, 'PAUSED')] : []
				presets[id] = btn(
					`Channel ${n}: ${text}`,
					`${text}\nCH ${n}`,
					[{ actionId: 'channel_cmd', options: { channel: String(n), command: cmd } }],
					fb,
					cmd === 'rec' ? DARKRED : bg,
				)
				ids.push(id)
			}
			const sel = `ch${n}_select`
			presets[sel] = btn(
				`Channel ${n}: select`,
				`SELECT\nCH ${n}`,
				[{ actionId: 'channel_cmd', options: { channel: String(n), command: 'select' } }],
				[{ feedbackId: 'channel_selected', options: { channel: String(n) }, style: { bgcolor: GREEN, color: WHITE } }],
			)
			ids.push(sel)
			groups.push(group(`g_ch${n}`, `Channel ${n}`, ids))
		}
		sections.push({ id: `channels_${first}`, name: `Channels ${first}-${first + 3}`, definitions: groups })
	}

	// ---- selected channels and the selection
	const transport = []
	for (const [cmd, text, bg] of COMMANDS) {
		const id = `sel_${cmd}`
		const fb = cmd === 'rec' ? [{ feedbackId: 'any_recording', options: {}, style: { bgcolor: RED, color: WHITE } }] : []
		presets[id] = btn(
			`Selected: ${text}`,
			`${text}\nSELECTED`,
			[{ actionId: 'selected_cmd', options: { command: cmd } }],
			fb,
			cmd === 'rec' ? DARKRED : bg,
		)
		transport.push(id)
	}
	const selection = []
	for (const [what, text] of [
		['all', 'SELECT\nALL'],
		['invert', 'INVERT\nSELECTION'],
		['none', 'SELECT\nNONE'],
	]) {
		const id = `selection_${what}`
		presets[id] = btn(`Selection: ${what}`, text, [{ actionId: 'selection', options: { what } }])
		selection.push(id)
	}
	presets.selection_count = btn('Selection: count', `SELECTED\n${v('selected_count')}`, [])
	selection.push('selection_count')
	sections.push({
		id: 'selected',
		name: 'Selected channels',
		definitions: [group('g_selected', 'Transport of the selected channels', transport), group('g_selection', 'Selection', selection)],
	})

	// ---- view
	const views = []
	for (const [view, text] of [
		['2x2', '2x2'],
		['3x3', '3x3'],
		['4x4', '4x4'],
		['list', 'LIST'],
	]) {
		const id = `view_${view}`
		presets[id] = btn(
			`View ${text}`,
			`VIEW\n${text}`,
			[{ actionId: 'view', options: { view } }],
			[{ feedbackId: 'view_is', options: { view }, style: { bgcolor: GREEN, color: WHITE } }],
		)
		views.push(id)
	}
	sections.push({ id: 'view', name: 'View', definitions: [group('g_view', 'Channel view', views)] })

	// ---- status
	presets.info_rec = btn(
		'Status: recording channels',
		`REC\n${v('rec_count')} / ${v('channels')}`,
		[],
		[{ feedbackId: 'any_recording', options: {}, style: { bgcolor: RED, color: WHITE } }],
	)
	presets.info_version = btn('Status: version', `ISO Rec\n${v('version')}`, [])
	sections.push({ id: 'status', name: 'Status', definitions: [group('g_status', 'Status', ['info_rec', 'info_version'])] })

	return { structure: sections, presets }
}
