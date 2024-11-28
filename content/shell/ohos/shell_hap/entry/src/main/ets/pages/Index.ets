// /*
//  * Copyright (c) 2023 Huawei Device Co., Ltd.
//  * Licensed under the Apache License, Version 2.0 (the "License");
//  * you may not use this file except in compliance with the License.
//  * You may obtain a copy of the License at
//  *
//  *     http://www.apache.org/licenses/LICENSE-2.0
//  *
//  * Unless required by applicable law or agreed to in writing, software
//  * distributed under the License is distributed on an "AS IS" BASIS,
//  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  * See the License for the specific language governing permissions and
//  * limitations under the License.
//  */

import { Logger } from '../utils/Logger';
import content_view from 'libcontent_shell_content_view.so';

@Entry
@Component
struct Index {
  @State shaftRotation: string = '';
  private url: string = 'https://webkit.org/demos/webgpu/';
  private xComponentAttrs : XComponentAttrs = {
    id: 'content_view',
    type: XComponentType.SURFACE,
    libraryname: 'content_shell_content_view',
  };
  private panOption: PanGestureOptions = new PanGestureOptions({ direction: PanDirection.All });

  async aboutToAppear() {
    Logger.info('aboutToAppear');
    let resourceManager = getContext(this).resourceManager;
    this.shaftRotation = await resourceManager.getStringValue($r('app.string.shaftRotation').id);
    content_view.setController(this);
  }

  build() {
    Column({space: 2}) {
      // Navigation
      TextInput({text: this.url})
        .onSubmit((enterKey: EnterKeyType, event: SubmitEvent) => {
          this.url = event.text;
          Logger.info('TextInput().onSubmit() url = ' + this.url);
          content_view.navigate(this.url);
        })
      Column() {
        XComponent(this.xComponentAttrs)
          .onLoad(() => {
            Logger.info('onLoad');
          })
          .height('100%')
          .width('100%')
          .onDestroy(() => {
            Logger.info('onDestroy');
          })
          .id('xComponent')
          .backgroundColor(Color.White)
      }
      .justifyContent(FlexAlign.SpaceAround)
      .alignItems(HorizontalAlign.Center)
      .height('100%')
      .width('100%')
      .backgroundColor(Color.White)
      .borderRadius(24)
    }
    .padding(2)
    .backgroundColor('#f1f3f5')
    .height('100%')
  }
}

interface XComponentAttrs {
  id: string;
  type: number;
  libraryname: string;
}