import { Octokit } from "@octokit/rest";
import fs, { createWriteStream } from "node:fs";
import { mkdtemp, readFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { Readable } from "stream";
import { finished } from "stream/promises";
import yazl from "yazl";

const owner = process.env.OWNER ?? "hello-nrfcloud";
const repo = process.env.REPO ?? "firmware";
const version = process.argv[process.argv.length - 1];

console.log(`Publishing release version`, version);

const octokit = new Octokit({
  auth: process.env.GITHUB_TOKEN,
});

console.log(`Repository: ${owner}/${repo}`);

const { data: release } = await octokit.rest.repos.getReleaseByTag({
  repo,
  owner,
  tag: version,
});

if (release === undefined) {
  console.error(`Release for ${version} not found!`);
  console.debug(`Got: ${releases.map(({ name }) => name).join(", ")}!`);
  process.exit(1);
}

const nameRegEx = new RegExp(
  `^hello\.nrfcloud\.com-${version}-thingy91x-((?<configuration>.+)-)?app_update_signed\.bin$`
);

const assets = release.assets.filter(
  ({ name }) => nameRegEx.exec(name) !== null
);

if (assets.length === 0) {
  console.error(`No assets found for release ${version}!`);
  console.debug(`Got: ${release.assets.map(({ name }) => name).join(", ")}!`);
  process.exit(1);
}

for (const asset of assets) {
  const { url, name, label, size } = asset;
  const {
    groups: { configuration },
  } = nameRegEx.exec(name);

  const fwversion = `${release.tag_name}${
    configuration !== undefined ? `-${configuration}` : ""
  }`;
  const fwName = `hello.nrfcloud.com ${fwversion}`;
  console.log(fwName);
  const manifest = {
    name: fwName,
    description: label,
    fwversion,
    "format-version": 1,
    files: [
      {
        file: name,
        type: "application",
        size,
      },
    ],
  };

  const res = await fetch(url, {
    headers: {
      Accept: "application/octet-stream",
      Authorization: `Bearer ${process.env.GITHUB_TOKEN}`,
    },
  });

  const zipfile = new yazl.ZipFile();
  zipfile.addBuffer(
    Buffer.from(JSON.stringify(manifest, null, 2), "utf-8"),
    "manifest.json"
  );

  const tempDir = await mkdtemp(path.join(os.tmpdir(), name));
  const fwFile = path.join(tempDir, "firmware.bin");

  const body = Readable.fromWeb(res.body);
  const download_write_stream = fs.createWriteStream(fwFile);
  await finished(body.pipe(download_write_stream));

  zipfile.addFile(fwFile, name);

  const zipFileName = path.join(tempDir, `${name}.zip`);

  await new Promise((resolve) => {
    zipfile.outputStream
      .pipe(createWriteStream(zipFileName))
      .on("close", () => {
        resolve();
      });
    zipfile.end();
  });

  const createFirmwareRes = await fetch(
    "https://api.nrfcloud.com/v1/firmwares",
    {
      method: "POST",
      headers: {
        Authorization: `Bearer ${process.env.NRF_CLOUD_API_KEY}`,
        "Content-Type": "application/zip",
      },
      body: await readFile(zipFileName),
    }
  );

  if (createFirmwareRes.status !== 200) {
    console.error(
      `Failed to upload firmware bundle: ${await createFirmwareRes.text()}`
    );
  } else {
    console.log(JSON.stringify(await createFirmwareRes.json(), null, 2));
  }
}
