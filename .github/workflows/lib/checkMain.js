module.exports = {
  async hasFixCiLabel(octokit, owner, repo, prNumber) {
    const labels = await octokit.rest.issues.listLabelsOnIssue({
      owner,
      repo,
      issue_number: prNumber,
    });

    return labels.data.map(label => label.name).includes('fix-ci');
  },

  async ensureChecksRunsSuccessful(delay, octokit, owner, repo) {
    let retryPending = 12; // Retry for up to 1 hour (12 retries with 5 minutes interval)

    while (retryPending > 0) {
      const mainRef = await octokit.rest.git.getRef({
        owner,
        repo,
        ref: 'heads/main',
      });

      const mainSha = mainRef.data.object.sha;
      console.log(`Main branch SHA: ${mainSha}`);

      let checkRuns = await octokit.rest.checks.listForRef({
        owner,
        repo,
        ref: mainSha,
      });

      console.log(`Check runs (${checkRuns.data.total_count}):`);
      for (let run of checkRuns.data.check_runs) {
        console.log(`- ${run.name} ${run.html_url}`);
        console.log(`\tstatus: ${run.status}`);
        console.log(`\tconclusion: ${run.conclusion}`);
      }

      const failedChecks = checkRuns.data.check_runs.filter(run => run.conclusion !== null && run.conclusion !== 'success');
      const pendingChecks = checkRuns.data.check_runs.filter(run => run.status !== 'completed');

      if (failedChecks.length > 0) {
        throw new Error('The main branch has failing checks.');
      } else if (pendingChecks.length === 0) {
        console.log('Main branch is healthy.');
        break;
      } else {
        console.log('Checks are still pending.');

        retryPending--;
        if (retryPending === 0) {
          throw new Error('Timeout waiting for pending checks on main branch.');
        }

        console.log('Waiting for 5 minutes before retrying...');
        await delay(5 * 60 * 1000); // Wait for 5 minutes
      }
    }
  },
};
