import { InstanceBase, InstanceStatus, Regex } from '@companion-module/base'
import { IsoRecorderClient, MAX_CHANNELS } from './client.js'
import { buildPresets, STATES } from './presets.js'

export const UpgradeScripts = []

const channelChoices = Array.from({ length: MAX_CHANNELS }, (_, i) => ({ id: String(i + 1), label: `Channel ${i + 1}` }))
const viewChoices = [
	{ id: '2x2', label: '2 x 2' },
	{ id: '3x3', label: '3 x 3' },
	{ id: '4x4', label: '4 x 4' },
	{ id: 'list', label: 'List' },
]
const transportChoices = [
	{ id: 'rec', label: 'REC (start recording)' },
	{ id: 'stop', label: 'STOP' },
	{ id: 'pause', label: 'PAUSE (pause / resume)' },
	{ id: 'split', label: 'SPLIT (close the file, start the next)' },
]

export default class IsoRecorderInstance extends InstanceBase {
	constructor(internal) {
		super(internal)
		this.client = new IsoRecorderClient()
	}

	async init(config) {
		this.config = config
		this.client.on('update', (changed) => this.onUpdate(changed))
		this.client.on('online', (on) => this.onOnline(on))
		this.client.on('error', (e) => this.updateStatus(InstanceStatus.ConnectionFailure, e.message))
		this.setActionDefinitions(this.actions())
		this.setFeedbackDefinitions(this.feedbacks())
		this.setVariableDefinitions(this.variables())
		const { structure, presets } = buildPresets(this.label)
		this.setPresetDefinitions(structure, presets)
		await this.connect()
	}

	async destroy() {
		this.client.removeAllListeners()
		this.client.close()
	}

	async configUpdated(config) {
		this.config = config
		await this.connect()
	}

	async connect() {
		this.updateStatus(InstanceStatus.Connecting)
		try {
			await this.client.open(this.config)
		} catch (e) {
			this.updateStatus(InstanceStatus.ConnectionFailure, e.message)
		}
	}

	getConfigFields() {
		return [
			{
				type: 'static-text',
				id: 'info',
				width: 12,
				label: 'Setup',
				value: 'In ISO Recorder open Settings → Remote control, tick "Listen for OSC commands" and enter the same port here.',
			},
			{ type: 'textinput', id: 'host', label: 'ISO Recorder address', width: 8, default: '127.0.0.1', regex: Regex.IP },
			{ type: 'number', id: 'port', label: 'OSC port', width: 4, default: 8000, min: 1, max: 65535 },
			{
				type: 'number',
				id: 'localPort',
				label: 'Local UDP port for the status (0 = automatic; set it to open the firewall for it)',
				width: 12,
				default: 0,
				min: 0,
				max: 65535,
			},
		]
	}

	onOnline(on) {
		if (on) this.updateStatus(InstanceStatus.Ok)
		else this.updateStatus(InstanceStatus.ConnectionFailure, 'ISO Recorder does not answer — is OSC switched on in Settings → Remote control?')
		this.setVariableValues({ online: on ? 1 : 0 })
		this.checkAllFeedbacks()
	}

	onUpdate(changed) {
		this.setVariableValues(changed)
		this.checkAllFeedbacks()
	}

	v(id) {
		return this.client.values[id]
	}

	actions() {
		const channel = { id: 'channel', type: 'dropdown', label: 'Channel', default: '1', choices: channelChoices }
		return {
			channel_cmd: {
				name: 'Channel: REC / STOP / PAUSE / SPLIT / SELECT',
				description: 'PAUSE pauses a recording and resumes a paused one. SELECT switches the selection of the channel.',
				options: [
					channel,
					{
						id: 'command',
						type: 'dropdown',
						label: 'Command',
						default: 'rec',
						choices: [...transportChoices, { id: 'select', label: 'SELECT (toggle)' }, { id: 'select/on', label: 'Select' }, { id: 'select/off', label: 'Deselect' }],
					},
				],
				callback: async ({ options }) => this.client.send(`/isorec/ch/${options.channel}/${options.command}`),
			},
			channel_toggle_rec: {
				name: 'Channel: REC if idle, STOP if recording',
				options: [channel],
				callback: async ({ options }) => {
					const rec = this.v(`ch${options.channel}_rec`) === 1 || this.v(`ch${options.channel}_paused`) === 1
					this.client.send(`/isorec/ch/${options.channel}/${rec ? 'stop' : 'rec'}`)
				},
			},
			selected_cmd: {
				name: 'Selected channels: REC / STOP / PAUSE / SPLIT',
				options: [{ id: 'command', type: 'dropdown', label: 'Command', default: 'rec', choices: transportChoices }],
				callback: async ({ options }) => this.client.send(`/isorec/selected/${options.command}`),
			},
			selection: {
				name: 'Selection: all / none / invert',
				options: [
					{
						id: 'what',
						type: 'dropdown',
						label: 'Selection',
						default: 'all',
						choices: [
							{ id: 'all', label: 'Select all' },
							{ id: 'none', label: 'Select none' },
							{ id: 'invert', label: 'Invert the selection' },
						],
					},
				],
				callback: async ({ options }) => this.client.send(`/isorec/select/${options.what}`),
			},
			view: {
				name: 'View: 2x2 / 3x3 / 4x4 / list',
				options: [{ id: 'view', type: 'dropdown', label: 'View', default: '4x4', choices: viewChoices }],
				callback: async ({ options }) => this.client.send(`/isorec/view/${options.view}`),
			},
		}
	}

	feedbacks() {
		const channel = { id: 'channel', type: 'dropdown', label: 'Channel', default: '1', choices: channelChoices }
		return {
			channel_state: {
				type: 'boolean',
				name: 'Channel state',
				description: 'True while the channel is in the chosen state',
				defaultStyle: { bgcolor: 0xcc0000, color: 0xffffff },
				options: [channel, { id: 'state', type: 'dropdown', label: 'State', default: 'REC', choices: STATES.map(([id, label]) => ({ id, label })) }],
				callback: ({ options }) => this.v(`ch${options.channel}_state`) === options.state,
			},
			channel_selected: {
				type: 'boolean',
				name: 'Channel is selected',
				defaultStyle: { bgcolor: 0x00802b, color: 0xffffff },
				options: [channel],
				callback: ({ options }) => this.v(`ch${options.channel}_selected`) === 1,
			},
			any_recording: {
				type: 'boolean',
				name: 'At least one channel is recording',
				defaultStyle: { bgcolor: 0xcc0000, color: 0xffffff },
				options: [],
				callback: () => Number(this.v('rec_count')) > 0,
			},
			view_is: {
				type: 'boolean',
				name: 'Current view',
				defaultStyle: { bgcolor: 0x00802b, color: 0xffffff },
				options: [{ id: 'view', type: 'dropdown', label: 'View', default: '4x4', choices: viewChoices }],
				callback: ({ options }) => this.v('view') === options.view,
			},
		}
	}

	variables() {
		const defs = {
			online: { name: 'Connection to ISO Recorder (1 = answers)' },
			version: { name: 'ISO Recorder version' },
			session: { name: 'Session name' },
			view: { name: 'Current view (2x2, 3x3, 4x4, list)' },
			channels: { name: 'Number of channels' },
			rec_count: { name: 'Number of recording channels' },
			selected_count: { name: 'Number of selected channels' },
		}
		for (let n = 1; n <= MAX_CHANNELS; n++) {
			defs[`ch${n}_name`] = { name: `Channel ${n}: name` }
			defs[`ch${n}_state`] = { name: `Channel ${n}: state (REC, PAUSED, PREVIEW, WAITING, RECONNECTING, ERROR, SAVING, OFF, NONE)` }
			defs[`ch${n}_time`] = { name: `Channel ${n}: recording time (HH:MM:SS)` }
			defs[`ch${n}_rec`] = { name: `Channel ${n}: recording (1/0)` }
			defs[`ch${n}_paused`] = { name: `Channel ${n}: paused (1/0)` }
			defs[`ch${n}_selected`] = { name: `Channel ${n}: selected (1/0)` }
			defs[`ch${n}_source`] = { name: `Channel ${n}: source` }
		}
		return defs
	}
}
