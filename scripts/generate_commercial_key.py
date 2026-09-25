#!/usr/bin/env python3
"""
Utility script to generate a cryptographically strong commercial release signing key
and output its SHA-256 digest for integration into BentoPack::LicenseManager.
"""

import secrets
import hashlib
import sys

def main():
    if len(sys.argv) > 1:
        token = sys.argv[1]
        print(f"Using provided token: {token}")
    else:
        # Generate 24 bytes (48 hex chars) of cryptographically secure random entropy
        token = "BP_COMMERCIAL_" + secrets.token_hex(24)
        print(f"Generated new BentoPack commercial signing key: {token}")

    digest = hashlib.sha256(token.encode('utf-8')).hexdigest()
    print(f"\n=======================================================")
    print(f"SHA-256 Digest for commercialgatekeeper.cpp: {digest}")
    print(f"=======================================================\n")
    print("How to configure in GitHub Repository Secrets:")
    print("  1. Go to repository: Settings -> Secrets and variables -> Actions")
    print("  2. Click 'New repository secret'")
    print(f"     Name:  COMMERCIAL_SIGNING_KEY")
    print(f"     Value: {token}")
    print("\n  3. (Optional) For Environment protection:")
    print("     Settings -> Environments -> Create 'commercial-release'")
    print("     Add 'Required reviewers' and add secret there.\n")

if __name__ == '__main__':
    main()
