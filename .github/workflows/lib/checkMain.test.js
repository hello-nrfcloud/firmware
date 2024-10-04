const { hasFixCiLabel, ensureChecksRunsSuccessful } = require('./checkMain');

// define class Octokit without depending on the original Octokit package. only the methods used in the test are defined.
class Octokit {
  constructor() {
    this.rest = {
      issues: {
        listLabelsOnIssue: jest.fn()
      },
      checks: {
        listForRef: jest.fn()
      },
      git: {
        getRef: jest.fn()
      }
    };
  }
}

function delay(ms) {
  return new Promise(resolve => resolve());
}

describe('check-main-status', () => {
  let octokit;
  const owner = 'test-owner';
  const repo = 'test-repo';
  const prNumber = 1;
  const mainSha = 'test-sha';

  beforeEach(() => {
    octokit = new Octokit();
    octokit.rest.git.getRef.mockResolvedValue({
      data: { object: { sha: mainSha } }
    });
  });

  describe('hasFixCiLabel', () => {
    it('should return true if the PR has the fix-ci label', async () => {
      octokit.rest.issues.listLabelsOnIssue.mockResolvedValue({
        data: [{ name: 'fix-ci' }]
      });

      const result = await hasFixCiLabel(octokit, owner, repo, prNumber);
      expect(result).toBe(true);
    });

    it('should return false if the PR does not have the fix-ci label', async () => {
      octokit.rest.issues.listLabelsOnIssue.mockResolvedValue({
        data: [{ name: 'other-label' }]
      });

      const result = await hasFixCiLabel(octokit, owner, repo, prNumber);
      expect(result).toBe(false);
    });
  });

  describe('ensureChecksRunsSuccessful', () => {
    beforeEach(() => {
      //octokit = new Octokit();
      octokit.rest.git.getRef.mockResolvedValue({
        data: { object: { sha: mainSha } }
      });
    });

    it('should not throw an error if all checks are successful', async () => {
      octokit.rest.checks.listForRef.mockResolvedValue({
        data: {
          total_count: 1,
          check_runs: [{ name: 'test-check', status: 'completed', conclusion: 'success' }]
        }
      });

      await expect(ensureChecksRunsSuccessful(delay, octokit, owner, repo, mainSha)).resolves.not.toThrow();
    });

    it('should throw an error if there are failing checks', async () => {
      octokit.rest.checks.listForRef.mockResolvedValue({
        data: {
          total_count: 1,
          check_runs: [{ name: 'test-check', status: 'completed', conclusion: 'failure' }]
        }
      });

      await expect(ensureChecksRunsSuccessful(delay, octokit, owner, repo, mainSha)).rejects.toThrow('The main branch has failing checks.');
    });

    it('should throw an error if checks are still pending after 12 retries', async () => {
      octokit.rest.checks.listForRef.mockResolvedValue({
        data: {
          total_count: 1,
          check_runs: [{ name: 'test-check', status: 'in_progress', conclusion: null }]
        }
      });

      await expect(ensureChecksRunsSuccessful(delay, octokit, owner, repo, mainSha)).rejects.toThrow('Timeout waiting for pending checks on main branch.');
      expect(octokit.rest.checks.listForRef).toHaveBeenCalledTimes(12);
    });

    it('should not throw an error if checks are successfull after pending 1 time', async () => {
      const pendingResponse = {
        data: {
          total_count: 1,
          check_runs: [{ name: 'test-check', status: 'in_progress', conclusion: null }]
        }
      };
      const successResponse = {
        data: {
          total_count: 1,
          check_runs: [{ name: 'test-check', status: 'completed', conclusion: 'success' }]
        }
      };

      octokit.rest.checks.listForRef
        .mockResolvedValueOnce(pendingResponse)
        .mockResolvedValueOnce(successResponse);

      await expect(ensureChecksRunsSuccessful(delay, octokit, owner, repo, mainSha)).resolves.not.toThrow();
      expect(octokit.rest.checks.listForRef).toHaveBeenCalledTimes(2);
    });

    it('should throw an error if there are failing checks after pending 1 time', async () => {
      const pendingResponse = {
        data: {
          total_count: 1,
          check_runs: [{ name: 'test-check', status: 'in_progress', conclusion: null }]
        }
      };
      const failureResponse = {
        data: {
          total_count: 1,
          check_runs: [{ name: 'test-check', status: 'completed', conclusion: 'failure' }]
        }
      };

      octokit.rest.checks.listForRef
        .mockResolvedValueOnce(pendingResponse)
        .mockResolvedValueOnce(failureResponse);

      await expect(ensureChecksRunsSuccessful(delay, octokit, owner, repo, mainSha)).rejects.toThrow('The main branch has failing checks.');
      expect(octokit.rest.checks.listForRef).toHaveBeenCalledTimes(2);
    });
  });
});
