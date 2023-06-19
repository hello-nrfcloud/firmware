import { Octokit } from '@octokit/rest'
import { createWriteStream } from 'node:fs'
import { mkdtemp, readFile } from 'node:fs/promises'
import os from 'node:os'
import path from 'node:path'
import yazl from 'yazl'

const octokit = new Octokit({
	auth: process.env.GITHUB_TOKEN,
})

const releases = await octokit.rest.repos.listReleases({
	owner: 'bifravst',
	repo: 'firmware',
})

const latestRelease = releases.data[0]

if (latestRelease === undefined) {
	console.error(`No release found!`)
	process.exit(1)
}

const release = latestRelease.tag_name

console.log('Release', release)

const nameRegEx = new RegExp(
	`^thingy91-asset_tracker_v2-((?<configuration>.+)-)?${release}-fwupd\.bin$`,
)

for (const asset of latestRelease.assets.filter(({ name }) =>
	name.endsWith('-fwupd.bin'),
)) {
	const { url, name, label, size } = asset
	const {
		groups: { configuration },
	} = nameRegEx.exec(name)

	const fwversion = `${release}${
		configuration !== undefined ? `-${configuration}` : ''
	}`
	const fwName = `hello.nrfcloud.com Firmware ${fwversion}`
	console.log(fwName)
	const manifest = {
		name: fwName,
		description: label,
		fwversion,
		'format-version': 1,
		files: [
			{
				file: name,
				type: 'application',
				size,
			},
		],
	}

	const res = await fetch(url, {
		headers: {
			Accept: 'application/octet-stream',
			Authorization: `Bearer ${process.env.GITHUB_TOKEN}`,
		},
	})

	const contents = await res.text()

	const zipfile = new yazl.ZipFile()

	zipfile.addBuffer(Buffer.from(contents, 'binary'), name)
	zipfile.addBuffer(
		Buffer.from(JSON.stringify(manifest, null, 2), 'utf-8'),
		'manifest.json',
	)

	const tempDir = await mkdtemp(path.join(os.tmpdir(), name))
	const zipFileName = path.join(tempDir, `${name}.zip`)

	await new Promise((resolve) => {
		zipfile.outputStream
			.pipe(createWriteStream(zipFileName))
			.on('close', () => {
				resolve()
			})
		zipfile.end()
	})

	const createFirmwareRes = await fetch(
		'https://api.nrfcloud.com/v1/firmwares',
		{
			method: 'POST',
			headers: {
				Authorization: `Bearer ${process.env.NRF_CLOUD_API_KEY}`,
				'Content-Type': 'application/zip',
			},
			body: await readFile(zipFileName),
		},
	)

	if (createFirmwareRes.status !== 200) {
		console.error(
			`Failed to upload firmware bundle: ${await createFirmwareRes.body()}`,
		)
	} else {
		console.log(JSON.stringify(await res.json(), null, 2))
	}
}
