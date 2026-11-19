/**
 * Pattern nRF Cloud's UploadFirmware API (POST /v1/firmwares) enforces on the
 * `fwversion` field of a bundle manifest. Values that don't match are rejected
 * with HTTP 422.
 */
export const fwversionRegEx = /^[a-zA-Z0-9._-]{1,60}$/;

/**
 * Builds the nRF Cloud `fwversion` for a release asset.
 *
 * @param {string} version Release version, e.g. "v3.0.1".
 * @param {string | undefined} configuration Build configuration taken from the
 *   asset filename, e.g. "debug". `undefined` for the default build.
 * @returns {string} "<version>" for the default build, otherwise
 *   "<version>-<configuration>", e.g. "v3.0.1-debug".
 * @throws {Error} If the result doesn't match `fwversionRegEx`.
 */
export const fwversionFor = (version, configuration) => {
  const fwversion =
    configuration === undefined ? version : `${version}-${configuration}`;
  if (!fwversionRegEx.test(fwversion)) {
    throw new Error(
      `Invalid fwversion "${fwversion}": nRF Cloud requires it to match ${fwversionRegEx}.`
    );
  }
  return fwversion;
};
