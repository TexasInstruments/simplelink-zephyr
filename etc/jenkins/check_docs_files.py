import argparse
import sys
import yaml
import subprocess

def read_yml_file(file_path: str) -> list[str]:
    """
    Read the yml file and return the file list.

    Args:
        file_path (str): The path to the yml file.

    Returns:
        A list of files to check.
    """
    with open(file_path, 'r') as file:
        data = yaml.safe_load(file)
        jobs = data.get('jobs', [])
        if jobs is None:
            sys.exit("Yaml file has unexpected format.")
        for step in jobs['doc-file-check']['steps']:
            if step.get('id') == 'check-doc-files':
                return step.get('with', {}).get('files', '').splitlines()


def check_file_changes(target_branch, files_to_check):
    """
    Check if a list of files or file patterns have changed based on a target branch.

    Args:
        target_branch (str): The target branch to compare against.
        files_to_check (list): A list of files or file patterns to check.

    Returns:
        A dictionary with the file names as keys and a boolean indicating whether the file has changed.
    """
    changes = []
    for file in files_to_check:
        # Use git diff to check for changes
        command = f"git diff --name-only {target_branch}..HEAD -- {file}"
        output = subprocess.check_output(command, shell=True)
        if output:
            changes.append(file)
    return changes


def main(target_branch, yml_file_path):
    # Read the yml file and get the file list
    files_to_check = read_yml_file(yml_file_path)

    # Check for file changes
    changes = check_file_changes(target_branch, files_to_check)

    # Print the results
    print(changes)
    if changes:
        print("Docs files have changed")
        sys.exit(1)
    else:
        sys.exit(0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Check if docs files have changed using a YML file.", allow_abbrev=False)
    parser.add_argument('--target-branch', required=True, help="The target branch to compare against.")
    parser.add_argument('--yml-file', default='.github/workflows/doc-build.yml', help="The path to the YML file.")
    args = parser.parse_args()
    main(args.target_branch, args.yml_file)
