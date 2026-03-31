/* This file is intended to be used only in TI's internal Continuous Integration
   (CI) system and will not function outside. */
import java.util.regex.Matcher
/* groovylint-disable DuplicateStringLiteral, NestedBlockDepth, UnnecessaryGetter */
/* groovylint-disable DuplicateNumberLiteral, CompileStatic */

env.FWTOOLS_TAG = '2025.12.18_0'
library("fwtools@${env.FWTOOLS_TAG}")

/* Command syntax help text
   Note: significant changes here must be applied to the PR template in Bitbucket settings
 */
String syntaxHelper = '''
## Syntax Help:
- `%% no_twister`
    - If present, do not build and run twister tests.
- `%% boards board [board board board ...]`
    - Space-separated list of boards that filters all other commands.
    - If empty, build for all boards.
    - Use %% boards all_supported_ti for all supported TI boards.
    - Example: %% boards lp_em_cc2340r5 lp_em_cc2745r10_q1/cc2745r10_q1
- `%% testfolders folder [folder folder folder ...]`
    - Space-separated list of folders to search for test cases in.
    - If omitted, defaults to nothing, which runs all tests.
    - Example: %% testfolders tests/drivers/entropy tests/subsys/random
- `%% no_docs`
    - If present, do not build docs. Otherwise do it if docs files have changed.
- `%% help`
    - Prints this message at the end of the build feedback.

Update your pull request to include any appropriate commands and run the job
again using the link at the top of this comment.
'''

pipeline
{
    environment
    {
        /* NOTE: In addition to these variables, Jenkins makes available a large number
           of bonus environment variables such as JOB_NAME. See a full list at:
           https://frwvosbuild.norway.design.ti.com/env-vars.html/ */
        CREDENTIALS_BITBUCKET_TOKEN = credentials('bitbucket-token-lprfsw')
        /* Documentation target, see doc/Makefile */
        DOC_TARGET = 'html-fast'

        /* Set up common docker compose args */
        DOCKER_COMPOSE_FILE = 'zephyr/etc/docker/docker-compose.yml'
        DOCKER_IMAGE_NAME = 'zephyr'
        /* These args come from .github/workflows/doc-build.yml and are used in the docs build step */
        DOCKER_SPHINX_ADDITIONAL_ARGS =
        '-e DOC_TAG=development -e SPHINXOPTS=\'-j8 -W -w docs_warnings.txt\
        --keep-going -T\' -e SPHINXOPTS_EXTRA=\'-t publish\''

        GUIDELINE_CHECK_FILE = 'guideline_check_output.txt'

        GIT_CONFIG_COUNT = 1

        GIT_CONFIG_KEY_0 = 'http.postBuffer'

        GIT_CONFIG_VALUE_0 = 524288000

        JENKINS_PYTHON_EXEC_NAME = 'python3.10'

        /* These args come from .github/workflows/twister.yml and are used in the twister step */

        SUPPORTED_BOARDS = 'lp_em_cc2340r5 lp_em_cc2340r53 lp_em_cc2745r10_q1/cc2745r10_q1 lp_em_cc2745r10_q1/cc2755r10'
        TEST_ALL_SUPPORTED_BOARDS = 'all_supported_ti'

        TEST_SUBDIRECTORY = '.'

        TWISTER_COMMON =
        '--force-color --inline-logs -v -N \
        --retry-failed 3 --timeout-multiplier 2 --clobber-output -W \
        -j $(nproc)'

        /* Avoid building large binary files */
        TWISTER_KCONFIG_OVERRIDES = '-x CONFIG_BUILD_OUTPUT_BIN=n'

        ZEPHYR_SDK_VERSION = '0.16.9'
    }

    parameters
    {
        string name: 'BOARDS',
            defaultValue: env.SUPPORTED_BOARDS,
            description: 'Space-separated list of boards to build and test on. If empty, build for all boards'

        string name: 'TESTFOLDERS',
            defaultValue: '',
            description: 'Space-separated list of folders to search for test cases in. If omitted, run all tests'

        text name: 'JOB_CONFIGURATION',
            defaultValue: '%% default',
            description: 'Configure your job as you would a pull request. \
            For pull requests, "%% commands" in the PR description are used.'
    }

    agent { label 'docker' }

    options {
        /* Skipped so we can clone zephyr into a subdirectory, to simplify west workspace */
        skipDefaultCheckout true
    }

    stages
    {
        stage('Setup')

        {
            steps
            {
                script
                {
                    common.init()
                    /* Clone main repo in subdirectory to use it as a west manifest repo */
                    dir('zephyr') {
                        common.nodeSetup()
                    }
                    /* Get keywords from pull request and override arguments with them if available
                       An empty map is returned if there are no keywords
                       An example map would be { "sysconfig": "", "test": "empty" } */
                    if (common.isPullRequest()) {
                        keywords = bitbucket.getPullRequestKeywords()
                    } else {
                        keywords = bitbucket.getKeywordsFromText(params.JOB_CONFIGURATION)
                    }

                    /* On first build of a pull request, deploy a feedback message
                       or if the user has %% help in their pull request */
                    if ((common.isPullRequest() && env.BUILD_NUMBER == '1') || keywords.containsKey('help')) {
                        env.FEEDBACK_TRAILER += syntaxHelper
                    }

                    /* Empty pull requests get default by default */
                    if (keywords == [:]) {
                        keywords['default'] = ''
                    }

                    /* Override default boards if available */
                    if (keywords.containsKey('boards')) {
                        if (keywords['boards'] == env.TEST_ALL_SUPPORTED_BOARDS) {
                            env.BOARDS_FINAL = env.SUPPORTED_BOARDS
                        }
                        else {
                            env.BOARDS_FINAL = keywords['boards']
                        }
                    } else {
                        env.BOARDS_FINAL = params.BOARDS
                    }

                    /* Don't pass -p if no boards specified */
                    if (env.BOARDS_FINAL != '') {
                        env.LAB_DEVICE_LABELS = env.BOARDS_FINAL.split(' ').collect { (it.split('/').last() + '_ZEPHYR').toUpperCase() }.join(' ')
                        env.BOARDS_FINAL = '-p ' + env.BOARDS_FINAL.split(' ').join(' -p ')
                    }

                    /* Override default test folders if available */
                    if (keywords.containsKey('testfolders')) {
                        env.TESTFOLDERS_FINAL = keywords['testfolders']
                    } else {
                        env.TESTFOLDERS_FINAL = params.TESTFOLDERS
                    }

                    /* Don't pass -T if no testfolders are specified */
                    if (env.TESTFOLDERS_FINAL != '') {
                        env.TESTFOLDERS_FINAL = '-T ' + env.TESTFOLDERS_FINAL.split(' ').join(' -T ')
                    }

                    docker_compose.build(args: "--build-arg ZEPHYR_SDK_VERSION=${env.ZEPHYR_SDK_VERSION}")

                    /* Setup west and get modules. Ensure west update is successful and remove modules/hal/ti to
                       ensure we are using the internal hal_ti module. West update is done twice since the first call
                       can fail for some modules. */
                    int westStatus = docker_compose.bashGetStatus('''\
                        git config --global url.https://lprfsw:\$BITBUCKET_AUTH_TOKEN@bitbucket.itg.ti.com/.insteadOf https://bitbucket.itg.ti.com/
                        if [ ! -d .west ]; then \
                            west init -l zephyr > west.init.log; \
                        fi ; \
                        west config manifest.group-filter -- +internal,-optional,-external ; \
                        west update -o=--depth=1 -n 2>&1 1> west.update.log || \
                        west update -o=--depth=1 -n 2>&1 1> west.update2.log
                        ''',
                        label: 'Create setup west environment'
                    )

                    sh('rm -rf modules/hal/ti')

                    /* Save west output as artifacts */
                    archiveArtifacts artifacts: 'west.*', allowEmptyArchive: true

                    if (westStatus != 0) {
                        common.printHeading(':warning: Environment setup failed')
                        common.printBody("Check west logs in build [artifacts](${env.RUN_ARTIFACTS_DISPLAY_URL})")
                        unstable("Build marked unstable due to west issues. Status code ${westStatus}")
                    }
                }
            }
        }

        stage('Compliance and Style Checks')

        {
            steps
            {
                script
                {
                    /* Run zephyr compliance tests as is done in .github/workflows/compliance.yml
                       ZEPHYR-163: fix checkpatch compliance failure */
                    if (env.CHANGE_TARGET != "null" && env.CHANGE_TARGET != null) {
                        int statusCompliance = docker_compose.bashGetStatus("""\
                          cd zephyr; \
                          source zephyr-env.sh; \
                          ./scripts/ci/check_compliance.py -e KconfigBasic -c origin/${env.CHANGE_TARGET}.. \
                        """, label: 'Run compliance checks')

                        /* Convert report to .html */
                        docker_compose.bash('junit2html zephyr/compliance.xml compliance_report.html')

                        /* Save converted report */
                        sh(
                            script: 'mkdir -p .compliance_reports; mv compliance_report.html .compliance_reports/',
                            label: 'Copy compliance reports'
                        )

                        /* Archive reports folder in Jenkins job */
                        publishHTML([
                            allowMissing: false, alwaysLinkToLastBuild: false, keepAll: false,
                            reportDir: './.compliance_reports', reportName: 'Compliance Reports',
                            reportFiles: 'compliance_report.html',
                            reportTitles: 'Compliance Report'
                        ])

                        /* Run coding guideline checks as is done in .github/workflows/coding_guidelines.yml */
                        int statusGuidelines = docker_compose.bashGetStatus("""\
                            touch ${env.GUIDELINE_CHECK_FILE}; \
                            cd zephyr; \
                            source zephyr-env.sh; \
                            ./scripts/ci/guideline_check.py \
                            --output ../${env.GUIDELINE_CHECK_FILE} \
                            -c origin/${env.CHANGE_TARGET}.. \
                        """, label: 'Run coding guideline checks')

                        /* ZEPHYR-165: run license check. It relies on an external github action
                        which is not trivial to replicate */

                        if (statusCompliance != 0) {
                            common.printHeading(':warning: Compliance Checks Failed')
                            common.printBody("Compliance results: ([Full report](${env.JOB_URL}/Compliance_20Reports/))")
                            unstable("Compliance check failures. Status code ${statusCompliance}")

                            dir('zephyr')
                            {
                                /* Check for jenkins merge commit */
                                String previousCommitMsg = sh(
                                script: 'git log -1 --pretty=format:"%s"',
                                returnStdout: true
                                ).trim()

                                // More specific pattern to extract the commit hash
                                Boolean matcher = previousCommitMsg ==~ /^Merge commit '([a-f0-9]{40})' into HEAD$/

                                if (matcher) {
                                    common.printBody("Jenkins has created a merge commit, causing some compliance check failures. Rebase your work on the target branch to fix this!")
                                }
                            }
                        } else {
                            common.printHeading(':white_check_mark: Compliance Checks Passed')
                        }
                        String guidelineCheckContents = readFile(env.GUIDELINE_CHECK_FILE)
                        if (statusGuidelines != 0 && guidelineCheckContents.size() != 0) {
                            common.printHeading(':warning: Coding Guideline Checks Failed')
                            common.printBody(guidelineCheckContents)
                            unstable("Coding guideline check failures. Status code ${statusGuidelines}")
                        } else {
                            common.printHeading(':white_check_mark: Coding Guideline Checks Passed')
                        }
                    }
                }
            }
        }

        stage('Docs')

        {
            steps
            {
                script
                {
                    /* Use the files listed in Zephyr's docs github action
                       to check if docs files have changed in this PR */
                    int statusDocsChanged = docker_compose.bashGetStatus("""\
                        cd zephyr ; \
                        ${env.JENKINS_PYTHON_EXEC_NAME} etc/jenkins/check_docs_files.py\
                        --target-branch origin/${env.CHANGE_TARGET} --yml-file .github/workflows/doc-build.yml \
                    """, label: 'Check if docs files have changed')

                    if (keywords.containsKey('no_docs')) {
                        if (statusDocsChanged != 0) {
                            common.printBody(':warning: Skipping docs but docs files have changed,\
                            please re-run build without the `%% no_docs` flag!')
                        } else {
                            common.printBody('Docs skipped')
                        }
                    } else if (statusDocsChanged != 0) {
                        /* Build docs as is done in .github/workflows/doc-build.yml */
                        common.printHeading(':books: Docs')

                        docker_compose.bash("""\
                            cd zephyr ; \
                            source zephyr-env.sh; \
                            printenv;
                            make -C doc ${env.DOC_TARGET}
                        """,additionalArgs: env.DOCKER_SPHINX_ADDITIONAL_ARGS, label: 'Build docs')

                        docker_compose.bash("""\
                            cd zephyr ; \
                            source zephyr-env.sh; \
                            ${env.JENKINS_PYTHON_EXEC_NAME} -m coverxygen --xml-dir  doc/_build/html/doxygen/xml/ \
                            --src-dir include/ --output doc-coverage.info; \
                            lcov --remove doc-coverage.info */deprecated > new.info; \
                            genhtml --no-function-coverage --no-branch-coverage new.info -o coverage-report \
                        """, label: 'Docs coverage')

                        sh('mkdir -p docs/build')
                        sh('cp -r zephyr/doc/_build/html/* docs/build')

                        publishHTML([
                            allowMissing: false,
                            alwaysLinkToLastBuild: false,
                            keepAll: false,
                            reportDir: './docs/build',
                            reportFiles: 'index.html',
                            reportName: 'Documentation',
                            reportTitles: ''
                        ])

                        common.printBody("[Click here to view updated documentation](${env.JOB_URL}/Documentation)")
                    }
                }
            }
        }

        stage('Twister')

        {
            steps
            {
                script
                {
                    if (keywords.containsKey('no_twister')) {
                        common.printHeading('Skipped Twister')
                    } else {
                        common.printHeading(':cyclone: Twister')
                        common.printHeading('Test Suite Builds')

                        /* Run twister tests as is done in .github/workflows/twister.yml
                           ZEPHYR-166: remove -W to treat warnings are error
                           Use pull_request_target parts of .github/workflows/twister.yml */
                        int statusTwister = docker_compose.bashGetStatus("""\
                            cd zephyr; \
                            source zephyr-env.sh; \
                            west twister --prep-artifacts-for-testing ${env.TWISTER_COMMON} ${env.TWISTER_KCONFIG_OVERRIDES} ${env.BOARDS_FINAL} ${env.TESTFOLDERS_FINAL}; \
                        """, label: 'Build twister tests')

                        if (statusTwister != 0) {
                            unstable("Build marked unstable due to twister failures. Status code ${statusTwister}")
                        }

                        stash(
                            name: 'firmware_images',
                            includes: ".west/**/*,zephyr/twister-out/**/*",
                            allowEmpty: true
                        )

                        /* Check out fwtools to have access to ./fwtools/scripts/jenkins/parse-xml-results.py */
                        git.checkoutHttps('lprfmw', 'fwtools', env.FWTOOLS_TAG)

                        String testResultTable = '''
| Test Results | Pass   | Fail   | Error | Skip    | Total |
|--------------|--------|--------|-------|---------|-------|
'''
                        /* Publish test summary to PR comment */
                        sh('cp zephyr/twister-out/twister.xml zephyr/twister-out/twister_build.xml')

                        testResultTable += common.parseMultiXmlResults('zephyr/twister-out/twister_build.xml')
                        common.printBody(testResultTable)

                        junit testResults: 'zephyr/twister-out/twister_build.xml',
                        allowEmptyResults: true, skipPublishingChecks: true

                        env.LOCATION = 'oslo'

                        /* Only mount devices when required */
                        common.printHeading('Tests Results')
                        test.allDevices(env.LAB_DEVICE_LABELS, '', additionalNodeSpec: env.LOCATION + "&& zephyr", twister: true)

                    }
                }
            }
        }
    }

    post
    {
        always
        {
            script
            {
                common.deployFeedback()
            }
        }
    }
}
