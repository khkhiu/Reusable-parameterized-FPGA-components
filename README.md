# Reusable parameterized FPGA components for onboard vegetation loss alerting on satellites
This project was undertaken as part of CDE2605R Undergraduate Research Experience (UREx), offered by the College of Design and Engineering (CDE) at the National University of Singapore (NUS).

## Abstract
Reconfigurable FPGA accelerators are promising for satellite edge computing. However, existing designs are typically optimized for one application, making them costly to change. This project designs reusable, parameterized pipelined components for onboard vegetation loss alerting. This involves flagging image tiles where vegetation indices drop sharply between two dates, so only changed tiles are downlinked, supporting deforestation monitoring from open Sentinel-2 data. Cloud masking and radiometric harmonisation are included as components. Reuse is tested across indices (NDVI, NIRv, NDWI); across learned classifiers (random forest, lightweight CNN) and at deployment, via partial bitstream regeneration for post-launch updates.

## Research Hypotheses
 Designing onboard accelerators as well-specified, parameterized components reduces the cost of changing them: both the engineering effort to produce a new configuration (H1a, H1b) and the resources needed to deploy it to the hardware (H2).
 
**H1a**
Pipelined vegetation loss alerting components (input conditioning (including radiometric harmonisation and cloud masking), spectral-index computation, and temporal change decision) will be designed as parameterized, well-specified, reusable modules. This approach is expected to reduce the development and verification effort required for new, **same-class configurations**, such as a different vegetation index (NIRv), a different spectral index and change direction (NDWI water gain), or a different precision. It should do so without a significant penalty in resource utilization or throughput compared to a hand-optimized, single-purpose design. This hypothesis tests problem 1(a).
 
**H1b**
The H1a methodology will be applied to two learned tile classifiers for the same vegetation loss alerting task. These are a random forest operating on tile statistics, and a lightweight CNN operating on the tile pair directly. The random forest is expected to reuse H1a components directly (feature extraction and threshold comparison). The CNN is expected to reuse only the input stage and the methodology (interface specifications, template conventions, verification structure). Both are expected to require less development and verification effort than from-scratch designs, with the reduction greater for the random forest. This hypothesis tests problem 1(b).
 
**H2**
The fixed component interfaces from H1a serve as reconfigurable-partition boundaries. Changing a single accelerator slot (e.g. NDVI loss alerting to NDWI water-gain alerting) via partial bitstream regeneration is expected to significantly reduce regeneration time, reconfiguration time and update size relative to full bitstream regeneration, at an acceptable area overhead. This hypothesis tests problem (2).
