import { fwversionFor } from "./fwversion.mjs";
import { nameRegEx } from "./nameRegEx.mjs";
import assert from "node:assert/strict";
import { describe, it } from "node:test";

describe("fwversionFor", () => {
  it("should use the version as-is for the default build", () =>
    assert.equal(fwversionFor("v3.0.1", undefined), "v3.0.1"));

  it("should join version and configuration with a dash", () =>
    assert.equal(fwversionFor("v3.0.1", "debug"), "v3.0.1-debug"));

  it("should build a valid fwversion from a release asset filename", () => {
    const {
      groups: { configuration },
    } = nameRegEx("v3.0.1").exec(
      "hello.nrfcloud.com-v3.0.1+debug-thingy91x-nrf91-update-signed.bin"
    );
    assert.equal(fwversionFor("v3.0.1", configuration), "v3.0.1-debug");
  });

  it("should reject a version containing a plus", () =>
    assert.throws(() => fwversionFor("v3.0.1+ci.1", undefined), {
      message: /Invalid fwversion "v3\.0\.1\+ci\.1"/,
    }));

  it("should reject an empty version", () =>
    assert.throws(() => fwversionFor("", undefined), {
      message: /Invalid fwversion ""/,
    }));

  it("should reject a fwversion longer than 60 characters", () =>
    assert.throws(() => fwversionFor("v".repeat(55), "debug"), {
      message: /Invalid fwversion/,
    }));
});
