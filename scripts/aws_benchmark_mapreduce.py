#!/usr/bin/env python3
"""Run MEngine benchmarks across short-lived EC2 workers and collect results."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import shlex
import subprocess
import sys
import tarfile
import tempfile
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
BENCHMARKS_DIR = REPO_ROOT / "benchmarks"
DEFAULT_ROLE = "mengine-bench-mapreduce-role"
DEFAULT_PROFILE = "mengine-bench-mapreduce-profile"
DEFAULT_SECURITY_GROUP = "mengine-bench-mapreduce"
DEFAULT_INSTANCE_TYPE = "t3.micro"

SOURCE_KEY = "source/mengine.tgz"


def run(cmd: list[str], *, json_out: bool = False, input_text: str | None = None) -> str | dict:
    proc = subprocess.run(
        cmd,
        input=input_text,
        text=True,
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        raise SystemExit(
            f"Command failed ({proc.returncode}): {shlex.join(cmd)}\n"
            f"stdout:\n{proc.stdout}\n\nstderr:\n{proc.stderr}"
        )
    if json_out:
        return json.loads(proc.stdout or "{}")
    return proc.stdout.strip()


def aws(args: list[str], *, region: str, json_out: bool = False) -> str | dict:
    cmd = ["aws", "--region", region, *args]
    if json_out and "--output" not in args:
        cmd += ["--output", "json"]
    return run(cmd, json_out=json_out)


def load_benchmark_names() -> list[str]:
    sys.path.insert(0, str(BENCHMARKS_DIR))
    from benchmarks.registry import ALL_BENCHMARKS

    return sorted(ALL_BENCHMARKS)


def parse_benchmarks(raw: str | None) -> list[str]:
    all_names = load_benchmark_names()
    if not raw:
        return all_names
    requested = [part.strip() for part in raw.split(",") if part.strip()]
    unknown = sorted(set(requested) - set(all_names))
    if unknown:
        raise SystemExit(f"Unknown benchmark(s): {', '.join(unknown)}")
    return requested


def account_id(region: str) -> str:
    ident = aws(["sts", "get-caller-identity"], region=region, json_out=True)
    return ident["Account"]


def default_vpc(region: str) -> str:
    data = aws(
        ["ec2", "describe-vpcs", "--filters", "Name=isDefault,Values=true"],
        region=region,
        json_out=True,
    )
    vpcs = data.get("Vpcs", [])
    if not vpcs:
        raise SystemExit("No default VPC found; pass --vpc-id and --subnet-ids.")
    return vpcs[0]["VpcId"]


def default_subnets(region: str, vpc_id: str) -> list[str]:
    data = aws(
        [
            "ec2",
            "describe-subnets",
            "--filters",
            f"Name=vpc-id,Values={vpc_id}",
            "Name=state,Values=available",
        ],
        region=region,
        json_out=True,
    )
    subnets = sorted(data.get("Subnets", []), key=lambda s: (s.get("AvailabilityZone", ""), s["SubnetId"]))
    if not subnets:
        raise SystemExit(f"No available subnets found in {vpc_id}; pass --subnet-ids.")
    return [s["SubnetId"] for s in subnets]


def discover_ami(region: str) -> str:
    data = aws(
        [
            "ec2",
            "describe-images",
            "--owners",
            "self",
            "--filters",
            "Name=tag:Project,Values=mengine",
            "Name=tag:Name,Values=mengine-bench-ami",
            "Name=state,Values=available",
        ],
        region=region,
        json_out=True,
    )
    images = data.get("Images", [])
    if not images:
        raise SystemExit(
            "Could not find a self-owned AMI tagged Project=mengine, Name=mengine-bench-ami. "
            "Build one from /home/jonros/mengine-buildimage or pass --ami-id."
        )
    images.sort(key=lambda img: img.get("CreationDate", ""), reverse=True)
    return images[0]["ImageId"]


def assert_free_tier_instance(region: str, instance_type: str):
    data = aws(
        [
            "ec2",
            "describe-instance-types",
            "--instance-types",
            instance_type,
            "--filters",
            "Name=free-tier-eligible,Values=true",
        ],
        region=region,
        json_out=True,
    )
    if not data.get("InstanceTypes"):
        raise SystemExit(
            f"{instance_type} is not marked free-tier eligible in {region}. "
            "Choose a free-tier eligible type or pass --no-free-tier-check."
        )


def warn_large_root_volume(region: str, ami_id: str, strict: bool):
    image = aws(["ec2", "describe-images", "--image-ids", ami_id], region=region, json_out=True)["Images"][0]
    total = 0
    for mapping in image.get("BlockDeviceMappings", []):
        ebs = mapping.get("Ebs")
        if ebs:
            total += int(ebs.get("VolumeSize", 0))
    if total > 30:
        msg = (
            f"AMI {ami_id} declares {total} GiB of EBS root/storage. "
            "That may exceed classic EC2 Free Tier storage allowance."
        )
        if strict:
            raise SystemExit(msg + " Rebuild the AMI smaller or drop --strict-ebs-free-tier.")
        print(f"warning: {msg}", file=sys.stderr)


def ensure_bucket(region: str, bucket: str | None, prefix: str, *, create: bool = False) -> str:
    if bucket and not create:
        return bucket
    bucket = bucket or generated_bucket_name(region)
    args = ["s3api", "create-bucket", "--bucket", bucket]
    if region != "us-east-1":
        args += ["--create-bucket-configuration", f"LocationConstraint={region}"]
    aws(args, region=region)
    aws(
        [
            "s3api",
            "put-public-access-block",
            "--bucket",
            bucket,
            "--public-access-block-configuration",
            "BlockPublicAcls=true,IgnorePublicAcls=true,BlockPublicPolicy=true,RestrictPublicBuckets=true",
        ],
        region=region,
    )
    print(f"Created s3://{bucket}/{prefix}")
    return bucket


def generated_bucket_name(region: str) -> str:
    stamp = dt.datetime.utcnow().strftime("%Y%m%d%H%M%S")
    return f"mengine-bench-{account_id(region)}-{region}-{stamp}".replace("_", "-")


def ensure_iam(region: str, bucket: str, prefix: str, role_name: str, profile_name: str):
    trust = {
        "Version": "2012-10-17",
        "Statement": [
            {
                "Effect": "Allow",
                "Principal": {"Service": "ec2.amazonaws.com"},
                "Action": "sts:AssumeRole",
            }
        ],
    }
    existing_roles = aws(["iam", "list-roles"], region=region, json_out=True).get("Roles", [])
    if role_name not in {role["RoleName"] for role in existing_roles}:
        aws(
            [
                "iam",
                "create-role",
                "--role-name",
                role_name,
                "--assume-role-policy-document",
                json.dumps(trust),
            ],
            region=region,
        )

    bucket_arn = f"arn:aws:s3:::{bucket}"
    object_arn = f"{bucket_arn}/{prefix}/*"
    policy = {
        "Version": "2012-10-17",
        "Statement": [
            {"Effect": "Allow", "Action": ["s3:ListBucket"], "Resource": bucket_arn},
            {
                "Effect": "Allow",
                "Action": ["s3:GetObject", "s3:PutObject"],
                "Resource": object_arn,
            },
        ],
    }
    aws(
        [
            "iam",
            "put-role-policy",
            "--role-name",
            role_name,
            "--policy-name",
            "mengine-bench-mapreduce-s3",
            "--policy-document",
            json.dumps(policy),
        ],
        region=region,
    )

    profiles = aws(["iam", "list-instance-profiles"], region=region, json_out=True).get(
        "InstanceProfiles", []
    )
    if profile_name not in {profile["InstanceProfileName"] for profile in profiles}:
        aws(["iam", "create-instance-profile", "--instance-profile-name", profile_name], region=region)

    profile = aws(
        ["iam", "get-instance-profile", "--instance-profile-name", profile_name],
        region=region,
        json_out=True,
    )["InstanceProfile"]
    if role_name not in {role["RoleName"] for role in profile.get("Roles", [])}:
        aws(
            [
                "iam",
                "add-role-to-instance-profile",
                "--instance-profile-name",
                profile_name,
                "--role-name",
                role_name,
            ],
            region=region,
        )
        print("Waiting for IAM instance profile propagation...")
        time.sleep(12)


def ensure_security_group(region: str, vpc_id: str, group_name: str) -> str:
    data = aws(
        [
            "ec2",
            "describe-security-groups",
            "--filters",
            f"Name=vpc-id,Values={vpc_id}",
            f"Name=group-name,Values={group_name}",
        ],
        region=region,
        json_out=True,
    )
    groups = data.get("SecurityGroups", [])
    if groups:
        return groups[0]["GroupId"]
    sg = aws(
        [
            "ec2",
            "create-security-group",
            "--group-name",
            group_name,
            "--description",
            "MEngine benchmark map-reduce workers; no inbound access",
            "--vpc-id",
            vpc_id,
        ],
        region=region,
        json_out=True,
    )
    return sg["GroupId"]


def ensure_s3_gateway_endpoint(region: str, vpc_id: str, bucket: str, prefix: str):
    service = f"com.amazonaws.{region}.s3"
    existing = aws(
        [
            "ec2",
            "describe-vpc-endpoints",
            "--filters",
            f"Name=vpc-id,Values={vpc_id}",
            f"Name=service-name,Values={service}",
            "Name=vpc-endpoint-type,Values=Gateway",
        ],
        region=region,
        json_out=True,
    ).get("VpcEndpoints", [])
    if existing:
        return

    route_tables = aws(
        ["ec2", "describe-route-tables", "--filters", f"Name=vpc-id,Values={vpc_id}"],
        region=region,
        json_out=True,
    ).get("RouteTables", [])
    route_table_ids = [rt["RouteTableId"] for rt in route_tables]
    if not route_table_ids:
        raise SystemExit(f"No route tables found in {vpc_id}; cannot create S3 gateway endpoint.")
    policy = {
        "Version": "2012-10-17",
        "Statement": [
            {
                "Effect": "Allow",
                "Principal": "*",
                "Action": ["s3:GetObject", "s3:PutObject", "s3:ListBucket"],
                "Resource": [f"arn:aws:s3:::{bucket}", f"arn:aws:s3:::{bucket}/{prefix}/*"],
            }
        ],
    }
    aws(
        [
            "ec2",
            "create-vpc-endpoint",
            "--vpc-id",
            vpc_id,
            "--service-name",
            service,
            "--vpc-endpoint-type",
            "Gateway",
            "--route-table-ids",
            *route_table_ids,
            "--policy-document",
            json.dumps(policy),
        ],
        region=region,
    )


def should_skip(path: Path) -> bool:
    rel = path.relative_to(REPO_ROOT)
    parts = rel.parts
    if not parts:
        return False
    if parts[0] in {".git", "build", ".cache"}:
        return True
    if parts[:2] in {("benchmarks", "results"), ("benchmarks", "plots"), ("benchmarks", "flame_trends")}:
        return True
    if "__pycache__" in parts:
        return True
    if path.suffix in {".o", ".pyc", ".tmp", ".log"}:
        return True
    return False


def package_source(out_path: Path):
    with tarfile.open(out_path, "w:gz") as tar:
        for path in REPO_ROOT.rglob("*"):
            if should_skip(path):
                continue
            tar.add(path, arcname=path.relative_to(REPO_ROOT), recursive=False)


def upload_source(region: str, bucket: str, prefix: str) -> str:
    with tempfile.TemporaryDirectory() as td:
        archive = Path(td) / "mengine.tgz"
        package_source(archive)
        key = f"{prefix}/{SOURCE_KEY}"
        run(["aws", "--region", region, "s3", "cp", str(archive), f"s3://{bucket}/{key}"])
        return key


def worker_user_data(args: argparse.Namespace, benchmark: str, source_key: str) -> str:
    engine_arg = f"--engine {shlex.quote(args.engine)}" if args.engine else ""
    force_arg = "--force" if args.force else ""
    bench_cmd = (
        f"python3 benchmarks/bench.py run {shlex.quote(benchmark)} --no-variants "
        f"--timeout {args.timeout:g} --trials {args.trials} --max-timeouts {args.max_timeouts} "
        f"--coq-timeout-multiplier {args.coq_timeout_multiplier:g} {engine_arg} {force_arg}"
    )
    max_seconds = int(args.max_worker_hours * 3600)
    config_json = json.dumps(
        {
            "mengine_path": "/home/ubuntu/mengine-mapreduce/repo/build/mengine",
            "mengine_root": "/home/ubuntu/mengine-mapreduce/repo",
            "coq_path": "coqc",
            "lean_path": "lean",
            "coqutil_root": "/home/ubuntu/src/coqutil",
            "results_dir": "results",
            "plots_dir": "plots",
            "default_timeout": args.timeout,
            "max_consecutive_timeouts": args.max_timeouts,
            "max_consecutive_failures": args.max_failures,
            "trials": args.trials,
            "coq_timeout_multiplier": args.coq_timeout_multiplier,
            "mengine_variants": {},
        },
        separators=(",", ":"),
    )
    return f"""#!/usr/bin/env bash
set -euo pipefail
exec > >(tee -a /var/log/mengine-benchmark-worker.log) 2>&1

BENCH={shlex.quote(benchmark)}
BUCKET={shlex.quote(args.bucket)}
PREFIX={shlex.quote(args.prefix)}
SOURCE_KEY={shlex.quote(source_key)}
REGION={shlex.quote(args.region)}
WORK=/home/ubuntu/mengine-mapreduce
STATUS=failed
STARTED_AT=$(date -u +%FT%TZ)

finish() {{
  rc=$?
  FINISHED_AT=$(date -u +%FT%TZ)
  mkdir -p "$WORK/status"
  cat >"$WORK/status/$BENCH.json" <<EOF
{{"benchmark":"$BENCH","status":"$STATUS","exit_code":$rc,"started_at":"$STARTED_AT","finished_at":"$FINISHED_AT","instance_id":"$(curl -fsS http://169.254.169.254/latest/meta-data/instance-id || true)"}}
EOF
  aws --region "$REGION" s3 cp "$WORK/status/$BENCH.json" "s3://$BUCKET/$PREFIX/status/$BENCH.json" || true
  if [[ -f "$WORK/repo/benchmarks/results/$BENCH.json" ]]; then
    aws --region "$REGION" s3 cp "$WORK/repo/benchmarks/results/$BENCH.json" "s3://$BUCKET/$PREFIX/results/$BENCH.json" || true
  fi
  shutdown -h now || true
}}
trap finish EXIT

export HOME=/home/ubuntu
export PATH="$HOME/.local/bin:$HOME/.elan/bin:$PATH"
source "$HOME/.elan/env" || true
eval "$(opam env --switch=coq-8.19 --set-switch)" || true

rm -rf "$WORK"
mkdir -p "$WORK"
aws --region "$REGION" s3 cp "s3://$BUCKET/$SOURCE_KEY" "$WORK/source.tgz"
mkdir -p "$WORK/repo"
tar -xzf "$WORK/source.tgz" -C "$WORK/repo"
cd "$WORK/repo"

python3 - <<'PY'
from pathlib import Path
Path("benchmarks/config.json").write_text({config_json!r})
PY

make clean || true
make -j"$(nproc)"

timeout --kill-after=30s {max_seconds}s bash -lc {shlex.quote(bench_cmd)}
STATUS=ok
"""


def launch_worker(
    args: argparse.Namespace,
    benchmark: str,
    source_key: str,
    subnet_id: str,
    security_group_id: str,
) -> str:
    public_ip = "true" if args.public_ip else "false"
    tags = (
        f"ResourceType=instance,Tags=[{{Key=Name,Value=mengine-bench-{benchmark}}},"
        "{Key=Project,Value=mengine},{Key=Role,Value=benchmark-worker},"
        f"{{Key=Benchmark,Value={benchmark}}},{{Key=RunId,Value={args.prefix}}}]"
    )
    volume_tags = (
        "ResourceType=volume,Tags=[{Key=Project,Value=mengine},"
        f"{{Key=Benchmark,Value={benchmark}}},{{Key=RunId,Value={args.prefix}}}]"
    )
    with tempfile.NamedTemporaryFile("w", delete=False) as user_data:
        user_data.write(worker_user_data(args, benchmark, source_key))
        user_data_path = user_data.name
    try:
        data = aws(
            [
                "ec2",
                "run-instances",
                "--image-id",
                args.ami_id,
                "--instance-type",
                args.instance_type,
                "--iam-instance-profile",
                f"Name={args.instance_profile}",
                "--instance-initiated-shutdown-behavior",
                "terminate",
                "--network-interfaces",
                f"DeviceIndex=0,SubnetId={subnet_id},Groups={security_group_id},AssociatePublicIpAddress={public_ip}",
                "--user-data",
                f"file://{user_data_path}",
                "--tag-specifications",
                tags,
                volume_tags,
                "--count",
                "1",
            ],
            region=args.region,
            json_out=True,
        )
    finally:
        Path(user_data_path).unlink(missing_ok=True)
    return data["Instances"][0]["InstanceId"]


def object_exists(region: str, bucket: str, key: str) -> bool:
    proc = subprocess.run(
        ["aws", "--region", region, "s3api", "head-object", "--bucket", bucket, "--key", key],
        text=True,
        capture_output=True,
        check=False,
    )
    return proc.returncode == 0


def instance_state(region: str, instance_id: str) -> str:
    data = aws(["ec2", "describe-instances", "--instance-ids", instance_id], region=region, json_out=True)
    return data["Reservations"][0]["Instances"][0]["State"]["Name"]


def collect_results(region: str, bucket: str, prefix: str):
    with tempfile.TemporaryDirectory() as td:
        remote = Path(td) / "results"
        remote.mkdir()
        subprocess.run(
            ["aws", "--region", region, "s3", "sync", f"s3://{bucket}/{prefix}/results/", str(remote)],
            check=False,
        )
        local = REPO_ROOT / "benchmarks" / "results"
        local.mkdir(parents=True, exist_ok=True)
        for result_path in remote.glob("*.json"):
            target = local / result_path.name
            incoming = json.loads(result_path.read_text())
            if target.exists():
                merged = json.loads(target.read_text())
                merged.update(incoming)
            else:
                merged = incoming
            target.write_text(json.dumps(merged, indent=2) + "\n")


def terminate_instances(region: str, ids: list[str]):
    if ids:
        aws(["ec2", "terminate-instances", "--instance-ids", *ids], region=region)


def orchestrate(args: argparse.Namespace):
    args.benchmarks = parse_benchmarks(args.benchmarks)
    args.prefix = args.prefix or dt.datetime.utcnow().strftime("mengine-mapreduce-%Y%m%d-%H%M%S")
    args.ami_id = args.ami_id or discover_ami(args.region)
    requested_bucket = args.bucket
    planned_bucket = requested_bucket or generated_bucket_name(args.region)

    estimated_hours = len(args.benchmarks) * args.max_worker_hours
    if estimated_hours > args.free_tier_hour_budget:
        raise SystemExit(
            f"Refusing to launch: worst-case {estimated_hours:.1f} instance-hours exceeds "
            f"--free-tier-hour-budget {args.free_tier_hour_budget:.1f}."
        )
    if not args.no_free_tier_check:
        assert_free_tier_instance(args.region, args.instance_type)
    warn_large_root_volume(args.region, args.ami_id, args.strict_ebs_free_tier)

    vpc_id = args.vpc_id or default_vpc(args.region)
    subnets = args.subnet_ids.split(",") if args.subnet_ids else default_subnets(args.region, vpc_id)
    if args.dry_run:
        print(f"AMI: {args.ami_id}")
        print(f"S3:  s3://{planned_bucket}/{args.prefix}/")
        print(f"VPC: {vpc_id}")
        print(f"Subnets: {', '.join(subnets)}")
        print(f"Instance type: {args.instance_type}")
        print(f"Max workers: {args.max_workers}")
        print(f"Worst-case instance-hours: {estimated_hours:.1f}")
        print(f"Benchmarks: {', '.join(args.benchmarks)}")
        print("Dry run complete; no resources created.")
        return

    args.bucket = ensure_bucket(args.region, planned_bucket, args.prefix, create=not requested_bucket)
    ensure_iam(args.region, args.bucket, args.prefix, args.iam_role, args.instance_profile)
    security_group_id = args.security_group_id or ensure_security_group(
        args.region, vpc_id, DEFAULT_SECURITY_GROUP
    )
    if not args.public_ip:
        ensure_s3_gateway_endpoint(args.region, vpc_id, args.bucket, args.prefix)
    source_key = upload_source(args.region, args.bucket, args.prefix)

    print(f"AMI: {args.ami_id}")
    print(f"S3:  s3://{args.bucket}/{args.prefix}/")
    print(f"Benchmarks: {', '.join(args.benchmarks)}")

    pending = list(args.benchmarks)
    running: dict[str, str] = {}
    failures: dict[str, str] = {}
    launched_ids: list[str] = []
    subnet_index = 0

    try:
        while pending or running:
            while pending and len(running) < args.max_workers:
                bench = pending.pop(0)
                subnet = subnets[subnet_index % len(subnets)]
                subnet_index += 1
                iid = launch_worker(args, bench, source_key, subnet, security_group_id)
                running[bench] = iid
                launched_ids.append(iid)
                print(f"launched {bench}: {iid}")

            time.sleep(args.poll_seconds)
            for bench, iid in list(running.items()):
                status_key = f"{args.prefix}/status/{bench}.json"
                if object_exists(args.region, args.bucket, status_key):
                    print(f"finished {bench}: {iid}")
                    del running[bench]
                    continue
                state = instance_state(args.region, iid)
                if state in {"shutting-down", "terminated", "stopped"}:
                    failures[bench] = f"{iid} ended before uploading status ({state})"
                    print(f"failed {bench}: {failures[bench]}", file=sys.stderr)
                    del running[bench]

        collect_results(args.region, args.bucket, args.prefix)
        if args.plot:
            run([sys.executable, str(REPO_ROOT / "benchmarks" / "bench.py"), "plot"])
        if failures:
            raise SystemExit("Some workers failed:\n" + "\n".join(f"{k}: {v}" for k, v in failures.items()))
    except KeyboardInterrupt:
        print("Interrupted; terminating launched instances...", file=sys.stderr)
        terminate_instances(args.region, launched_ids)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--region", default=os.environ.get("AWS_REGION", "us-east-1"))
    parser.add_argument("--ami-id", help="MEngine benchmark AMI; defaults to latest self-owned tagged AMI")
    parser.add_argument("--instance-type", default=DEFAULT_INSTANCE_TYPE)
    parser.add_argument("--benchmarks", help="Comma-separated benchmark names; default: all")
    parser.add_argument("--max-workers", type=int, default=2)
    parser.add_argument("--max-worker-hours", type=float, default=6.0)
    parser.add_argument("--free-tier-hour-budget", type=float, default=700.0)
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--trials", type=int, default=2)
    parser.add_argument("--max-timeouts", type=int, default=2)
    parser.add_argument("--max-failures", type=int, default=2)
    parser.add_argument("--coq-timeout-multiplier", type=float, default=1.5)
    parser.add_argument("--engine", help="Optional engine filter, e.g. mengine,coq,lean")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--bucket", help="Existing S3 bucket for source/results")
    parser.add_argument("--prefix", help="S3 prefix/run id")
    parser.add_argument("--vpc-id")
    parser.add_argument("--subnet-ids", help="Comma-separated subnet ids")
    parser.add_argument("--security-group-id")
    parser.add_argument("--iam-role", default=DEFAULT_ROLE)
    parser.add_argument("--instance-profile", default=DEFAULT_PROFILE)
    parser.add_argument("--public-ip", action="store_true", help="Use public IPv4 instead of an S3 VPC endpoint")
    parser.add_argument("--no-free-tier-check", action="store_true")
    parser.add_argument("--strict-ebs-free-tier", action="store_true")
    parser.add_argument("--poll-seconds", type=int, default=30)
    parser.add_argument("--plot", action="store_true", help="Run local plotting after collecting results")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if args.max_workers < 1:
        raise SystemExit("--max-workers must be at least 1")
    orchestrate(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
