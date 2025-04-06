APP_BUNDLE_NAME = "org.chromium.content_shell"
OUT = ${HOME}/sources/chromium/src/out/Release

OHOS_COMMAND_LINE_TOOLS_PATH = ${HOME}/sources/command-line-tools
OHOS_TOOLCHAINS_PATH = ${OHOS_COMMAND_LINE_TOOLS_PATH}/sdk/default/openharmony/toolchains
SIGN_TOOL_PATH = ${OHOS_COMMAND_LINE_TOOLS_PATH}/sdk/default/openharmony/toolchains/lib

MKDIR = mkdir -p
RM = rm -rf
HVIGORW = ${OHOS_COMMAND_LINE_TOOLS_PATH}/bin/hvigorw
JAVA = java
HAP_SIGN_TOOL = ${JAVA} -jar ${SIGN_TOOL_PATH}/hap-sign-tool.jar

all:

v8:
	ninja -C $(OUT) v8

v8_unittests:
	ninja -C $(OUT) v8_unittests

content_shell:
	ninja -C $(OUT) content_shell

signed-profile: openharmony_sx.p7b

unsigned-hap: entry/build/default/outputs/default/entry-default-unsigned.hap

entry/build/default/outputs/default/entry-default-unsigned.hap: .libs.stamp .resfile.stamp
	${HVIGORW} assembleHap

openharmony_sx.p7b: UnsgnedReleasedProfileTemplate.json
	${HAP_SIGN_TOOL} sign-profile \
		-keyAlias "openharmony application profile release" \
		-signAlg "SHA256withECDSA" \
		-mode "localSign" \
		-profileCertFile "${SIGN_TOOL_PATH}/OpenHarmonyProfileRelease.pem" \
		-inFile "UnsgnedReleasedProfileTemplate.json" \
		-keystoreFile "${SIGN_TOOL_PATH}/OpenHarmony.p12" \
		-outFile "openharmony_sx.p7b" \
		-keyPwd "123456" \
		-keystorePwd "123456"

signed-hap: entry-default-signed.hap

entry-default-signed.hap: entry/build/default/outputs/default/entry-default-unsigned.hap openharmony_sx.p7b
	${HAP_SIGN_TOOL} sign-app \
		-keyAlias "openharmony application release" \
		-signAlg "SHA256withECDSA" \
		-mode "localSign" \
		-appCertFile "OpenHarmonyApplication.pem" \
		-inFile "entry/build/default/outputs/default/entry-default-unsigned.hap" \
		-keystoreFile "${SIGN_TOOL_PATH}/OpenHarmony.p12" \
		-profileFile "openharmony_sx.p7b" \
		-keyPwd "123456" \
		-keystorePwd "123456" \
		-outFile "entry-default-signed.hap"

install: entry-default-signed.hap
	hdc install entry-default-signed.hap

stop:
	hdc shell aa force-stop ${APP_BUNDLE_NAME}

run:
	hdc shell hilog -b D -T chromium
	# hdc shell hilog -r
	hdc shell aa start -b ${APP_BUNDLE_NAME} -a EntryAbility
	hdc shell hilog -T chromium,Faultlogger | tee log
	# hdc hilog | tee log
	# hdc shell "hilog -r && hilog -p off && hilog -b F && hilog -b E -T chromium && hilog -b I -T Faultlogger && hilog"

libs .libs.stamp: ${OUT}/libcontent_shell_content_view.so ${OUT}/libtest_trace_processor.so
	${MKDIR} entry/libs/arm64-v8a
	cp ${OUT}/libcontent_shell_content_view.so entry/libs/arm64-v8a
	cp ${OUT}/libtest_trace_processor.so entry/libs/arm64-v8a
	touch .libs.stamp

resfile .resfile.stamp:
	${MKDIR} AppScope/resources/resfile/
	cp ${OUT}/icudtl.dat AppScope/resources/resfile/
	cp ${OUT}/v8_context_snapshot.bin AppScope/resources/resfile/
	cp ${OUT}/content_shell.pak AppScope/resources/resfile/
	touch .resfile.stamp

clean:
	${HVIGORW} clean
	${RM} \
		entry-default-signed.hap \
		openharmony_sx.p7b \
		.resfile.stamp \
		.libs.stamp
