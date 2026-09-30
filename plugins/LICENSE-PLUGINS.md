# BentoPack Source-Available & Commercial Licensing Agreement (EULA)

This End-User License Agreement ("EULA") governs the source code, binaries, gatekeepers, forensic integrity systems, engine exporters, and proprietary plugins (collectively, the "Software") of **BentoPack**, including but not limited to:
* All components located within the `plugins/` directory (filters, extractors, engine bridges, commercial gatekeeper);
* The License Management and Forensic Integrity subsystems located in `BentoPack/include/license/` and `BentoPack/src/license/` (`LicenseManager`, `IntegrityGuard`, `ILicenseGatekeeper`);
* All integrated distributions, binaries, and containerized tools (GUI and CLI) combining the core engine with the aforementioned components.

The core algorithmic packing primitives (MaxRects, basic data models) remain licensed under the Apache License 2.0 as stated in the root `LICENSE` file. All other features, forensic safeguards, and engine exporters are strictly governed by this EULA.

By downloading, compiling, accessing, integrating, or running the Software, you agree to be legally bound by the terms and conditions of this EULA.

---

### 1. The $1,000,000 USD Consolidated Revenue Threshold

This agreement provides a royalty-free, source-available license for independent creators, students, and businesses operating below the commercial threshold:

* **Royalty-Free Tier (Community):** You are granted a non-exclusive, non-transferable license to compile from source, modify for internal use, and use the Software to create digital game assets, provided that your **Consolidated Annual Gross Revenue** (or total funding received) does not exceed **$1,000,000 USD** (One Million United States Dollars) within the preceding 12-month period.
* **Consolidated Group Definition (Anti-Shell Entity Clause):** "Consolidated Annual Gross Revenue" includes the aggregate gross revenues and funding of:
  1. The licensee individual or legal entity;
  2. Any parent company, holding company, or ultimate beneficial owner controlling more than 20% equity or voting rights;
  3. Any subsidiaries, corporate affiliates, or sister companies under common control;
  4. Any third-party publishing entity, distributor, or venture capital partner funding, publishing, or contractually co-producing the game or application in which the Software's output is utilized.
* If a project is published, co-developed, or financed by a publisher or corporate group whose consolidated revenue exceeds $1,000,000 USD, the project is strictly ineligible for the Royalty-Free Tier, regardless of the individual development studio's revenue.

---

### 2. Commercial & Enterprise License Requirement

If your Consolidated Annual Gross Revenue exceeds the $1,000,000 USD threshold, your rights under the Royalty-Free Tier immediately and automatically terminate. To continue using the Software, you must obtain an official **BentoPack Commercial / Enterprise License** from the copyright holder before any commercial deployment or production release.

Commercial licenses unlock:
* Unrestricted commercial production rights across all group entities;
* Elimination of resolution bounds (unlocking 8K and 16K atlas generation);
* Commercial release token and gatekeeper certification;
* Direct support, SLA guarantees, and priority feature requests.

---

### 3. Pre-Compiled Binaries & Convenience Purchases

This EULA governs the source code and self-compiled instances of the Software.
Pre-compiled binaries distributed via authorized digital distribution channels (such as Steam, Itch.io, Epic Fab, or Unity Asset Store) are sold under standard convenience licensing terms granting immediate single-seat or team usage without requiring local compilation or toolchain setup.

---

### 4. Restrictions on Redistribution, Circumvention & Copyright Management Information (CMI)

You may freely use the Software to generate output assets (sprite sheets, KTX2 textures, polygon meshes, animations, JSON/engine descriptors) and you retain full intellectual property rights in your original artistic content.

However, the following actions are strictly prohibited:

1. **No Redistribution of the Software:** You may not redistribute, sub-license, rent, lease, or resell the Software (source code or compiled binaries), nor incorporate it into a competing Software-as-a-Service (SaaS) or commercial asset packaging tool.
2. **Statutory Copyright Management Information (CMI):**
   * All metadata tags, text chunks, and cryptographic signatures injected by the Software into generated files (including but not limited to `X-BentoPack-Integrity`, `X-BentoPack-License`, `X-BentoPack-Tool`, `X-BentoPack-Notice`, PNG `tEXt` chunks, KTX2 key-value dictionaries, JSON descriptor fields, and layout HMAC hashes) constitute **Copyright Management Information (CMI)** under:
     - Section 1202 of Title 17 of the United States Code (17 U.S.C. § 1202 - Digital Millennium Copyright Act);
     - Article 12 of the WIPO Copyright Treaty (WCT);
     - Article 7 of the European Union Directive 2001/29/EC and Directive 2009/24/EC on the legal protection of computer programs;
     - Applicable provisions of the Copyright Law of Japan.
   * You shall NOT intentionally remove, strip, obscure, alter, or falsify any such CMI, nor distribute or publicly deploy works containing altered or stripped CMI, with knowledge or reasonable grounds to know that it will induce, enable, facilitate, or conceal copyright infringement.
   * Under 17 U.S.C. § 1203 and equivalent international laws, unauthorized removal or alteration of CMI gives rise to statutory damages of up to **$25,000 USD per violation**, in addition to actual damages, profits, and attorneys' fees.
3. **Anti-Circumvention:** You shall not bypass, disable, tamper with, reverse engineer, decompile, or modify the gatekeeper logic (`CommercialGatekeeper`), integrity verification routines (`IntegrityGuard`), or license validation code (`LicenseManager`). Any attempt to compile binaries with deactivated gatekeeper checks or stripped CMI tags constitutes a material breach of this EULA resulting in immediate and irrevocable termination of all usage rights.

---

### 5. Audit Rights & Forensic Verification

The Licensor reserves the right to verify compliance with this EULA.
Because BentoPack forensic metadata and layout signatures persist non-destructively within generated game assets and runtime package archives (e.g., Unity `.assets`, Godot `.pck`, Unreal `.pak`), the presence of BentoPack CMI or layout signatures within a commercial production whose consolidated publisher or developer exceeds the $1,000,000 USD revenue threshold without a valid Commercial License shall constitute *prima facie* evidence of unlicensed commercial use.

In the event of an audit revealing unlicensed use, the offending party shall immediately pay standard commercial license fees retroactively, subject to statutory interest and full reimbursement of all audit and legal fees incurred.

---

### 6. Disclaimer of Warranty

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY ARISING FROM THE USE OF THE SOFTWARE.

---

### 7. Governing Law and Jurisdiction

This EULA shall be governed by and construed in accordance with the laws of Japan, without regard to conflict of law principles. Any dispute, controversy, or claim arising out of or relating to this EULA, its breach, or the unauthorized use of the Software shall be subject to the exclusive jurisdiction of the Tokyo District Court, Japan.