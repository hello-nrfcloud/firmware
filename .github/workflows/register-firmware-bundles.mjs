import fs, { createWriteStream } from "node:fs";
import { readFile } from "node:fs/promises";
import { mkdtemp } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import yazl from "yazl";
import { fwversionFor } from "./lib/fwversion.mjs";
import { nameRegEx } from "./lib/nameRegEx.mjs";

const version = (process.argv[process.argv.length - 1] ?? "").trim();
const filenameRegEx = nameRegEx(version);

console.log(`Publishing release version`, version);

const assets = fs
  .readdirSync(process.cwd())
  .filter((name) => filenameRegEx.test(name));

if (assets.length === 0) {
  console.error(`No assets found for release ${version}!`);
  process.exit(1);
}

const bundles = [];
for (const asset of assets) {
  const {
    groups: { configuration },
  } = filenameRegEx.exec(asset);
  try {
    bundles.push({
      asset,
      configuration,
      fwversion: fwversionFor(version, configuration),
    });
  } catch (error) {
    console.error(`${asset}: ${error.message}`);
    process.exit(1);
  }
}

const createBundleZip = async ({ asset, configuration, fwversion }) => {
  const fwName = `hello.nrfcloud.com ${fwversion}`;
  console.log(fwName);
  const manifest = {
    name: fwName,
    description: `Firmware update for the Thingy:91 X (${
      configuration ?? "default"
    })`,
    fwversion,
    "format-version": 1,
    files: [
      {
        file: asset,
        type: "application",
        size: fs.statSync(asset).size,
      },
    ],
  };

  console.log(JSON.stringify(manifest, null, 2));

  const zipfile = new yazl.ZipFile();
  zipfile.addBuffer(
    Buffer.from(JSON.stringify(manifest, null, 2), "utf-8"),
    "manifest.json"
  );

  const tempDir = await mkdtemp(path.join(os.tmpdir(), asset));

  zipfile.addFile(path.join(process.cwd(), asset), asset);

  const zipFileName = path.join(tempDir, `${asset}.zip`);

  await new Promise((resolve, reject) => {
    zipfile.outputStream
      .pipe(createWriteStream(zipFileName))
      .on("close", resolve)
      .on("error", reject);
    zipfile.end();
  });

  return zipFileName;
};

const uploadBundle = async (zipFileName) => {
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

  if (!createFirmwareRes.ok) {
    throw new Error(
      `HTTP ${createFirmwareRes.status}: ${await createFirmwareRes.text()}`
    );
  }
  console.log(JSON.stringify(await createFirmwareRes.json(), null, 2));
};

const failedAssets = [];
for (const bundle of bundles) {
  try {
    await uploadBundle(await createBundleZip(bundle));
  } catch (error) {
    console.error(
      `Failed to upload firmware bundle for ${bundle.asset}: ${error.message}`
    );
    failedAssets.push(bundle.asset);
  }
}

if (failedAssets.length > 0) {
  console.error(
    `${failedAssets.length} of ${bundles.length} firmware bundles failed to upload: ${failedAssets.join(", ")}`
  );
  process.exit(1);
}
