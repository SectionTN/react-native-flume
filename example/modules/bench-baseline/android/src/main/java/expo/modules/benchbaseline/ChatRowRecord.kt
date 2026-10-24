package expo.modules.benchbaseline

import expo.modules.kotlin.records.Field
import expo.modules.kotlin.records.Record

class ChatRowRecord : Record {
  @Field val id: String = ""
  @Field val kind: String = "message"
  @Field val text: String = ""
  @Field val time: String = ""
  @Field val avatar: String = ""
  @Field val duration: String? = null
}
