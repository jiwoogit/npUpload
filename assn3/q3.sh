#!/bin/bash
awk '$1=="FrameConsumerLog::RemainFrames:" {print $2}' log.out > ~/gitUpload/assn3/q2.out
awk '$1=="FrameConsumerLog::RemainFrames:" {print $2}' log1.out > ~/gitUpload/assn3/q31.out
awk '$1=="FrameConsumerLog::RemainFrames:" {print $2}' log2.out > ~/gitUpload/assn3/q32.out
awk '$1=="FrameConsumerLog::RemainFrames:" {print $2}' log3.out > ~/gitUpload/assn3/q33.out
awk '$1=="FrameConsumerLog::RemainFrames:" {print $2}' log4.out > ~/gitUpload/assn3/q34.out
awk '$1=="FrameConsumerLog::RemainFrames:" {print $2}' log5.out > ~/gitUpload/assn3/q35.out

