#!/bin/bash
awk '$1=="FrameConsumerLog::RemainFrames:" {print $2}' log.out > q2.out

